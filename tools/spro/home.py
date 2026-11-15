# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""sedona.home overlay and sedonac steps shared by all targets.

build/home is the sedona.home: lib/ (jars + sedona.properties), kits/ and
manifests/ (sedonac output) and repo -> the repository, so native sources
are referenced as /repo/<path> in platform manifests.
"""
import base64
import copy
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import xml.etree.ElementTree as ET
import zipfile

from .project import ROOT, PROJECT, closure, kits as all_kits

BUILD = ROOT / 'build'
HOME = BUILD / 'home'
JAVA_MODULES = ('sedona', 'sedonac', 'sedonacert')


def run(args, **kw):
    args = [str(a) for a in args]
    shown = args if len(args) < 24 else args[:3] + ['... %d more args ...' % (len(args) - 6)] + args[-3:]
    print('+', shlex.join(shown), flush=True)
    subprocess.run(args, check=True, **kw)


def sedona_version():
    props = (ROOT / 'core/lib/sedona.properties').read_text().splitlines()
    return next(l.split('=', 1)[1].strip() for l in props if l.startswith('buildVersion='))


def overlay():
    for d in ('lib', 'kits', 'manifests', 'platforms/db'):
        (HOME / d).mkdir(parents=True, exist_ok=True)
    link = HOME / 'repo'
    if not link.is_symlink():
        link.symlink_to(ROOT)
    shutil.copy2(ROOT / 'core/lib/sedona.properties', HOME / 'lib/sedona.properties')


def sedonac(*args):
    run(['java', '-Dsedona.home=' + str(HOME), '-Dsedona.buildHost=sedonapro', '-cp',
         os.pathsep.join(str(HOME / 'lib' / (m + '.jar')) for m in ('sedona', 'sedonac')),
         'sedonac.Main', *args])


def jars(force=False):
    overlay()
    for m in JAVA_MODULES:
        jar = HOME / 'lib' / (m + '.jar')
        src = ROOT / 'core/java' / m / 'src'
        sources = sorted(src.rglob('*.java'))
        if not force and jar.exists() and jar.stat().st_mtime > max(p.stat().st_mtime for p in sources):
            continue
        classes = BUILD / 'java' / m
        if classes.exists():
            shutil.rmtree(classes)
        classes.mkdir(parents=True)
        argfile = BUILD / (m + '-javac.args')
        argfile.write_text('\n'.join('"%s"' % p for p in sources))
        cp = os.pathsep.join(str(p) for p in (HOME / 'lib').glob('*.jar')) or '.'
        run(['javac', '-nowarn', '-encoding', 'UTF-8', '-source', '8', '-target', '8',
             '-classpath', cp, '-d', classes, '@' + str(argfile)])
        if m == 'sedona':
            (classes / 'version.txt').write_text('version=%s\n' % sedona_version())
        run(['jar', 'cf', jar, '-C', classes, '.'])


def compile_kits(names=None):
    """Compile kits (default: all) and their dependencies into build/home/kits."""
    jars()
    known = all_kits()
    for n in closure(names or sorted(known), known):
        k = known[n]
        newest = max(p.stat().st_mtime for p in k.dir.rglob('*') if p.is_file())
        out = list((HOME / 'kits' / n).glob('*.kit'))
        if out and max(p.stat().st_mtime for p in out) > newest:
            continue
        sedonac(k.dir / 'kit.xml')


def platform_manifest(target, kit_names, known, outdir, refsize, plat_id, extra_sources=()):
    """Platform manifest: native kits + native sources of exactly kit_names."""
    root = ET.Element('sedonaPlatform', vendor=PROJECT['vendor'], id=plat_id)
    comp = ET.SubElement(root, 'compile', endian='little', blockSize='4', refSize=str(refsize),
                         debug='true', test='true')
    for n in kit_names:
        if known[n].natives:
            ET.SubElement(comp, 'nativeKit', depend=n + ' 1.2+')
    for src in ['/repo/core/vm', *extra_sources]:
        ET.SubElement(comp, 'nativeSource', path=src)
    for n in kit_names:
        for src in known[n].sources(target):
            ET.SubElement(comp, 'nativeSource', path=src)
    inc = ET.SubElement(ET.SubElement(root, 'manifestInclude'), 'schemes')
    for s in 'mkp':
        ET.SubElement(inc, 'scheme', id=s, transfer='single')
    ET.indent(root)
    path = outdir / 'platform.xml'
    ET.ElementTree(root).write(path, encoding='unicode')
    return path


def natives(manifest, out, board=None):
    """Stage native sources; with a board, also its channel table spro_board.c."""
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    sedonac(manifest, '-outDir', out)
    if board:
        from . import boards
        boards.write_c(board, out)
    print('staged %d C files in %s' % (len(list(out.glob('*.c'))), out))


def board_for(target, names, board_name):
    """(board or None, extra native sources) for a target build."""
    from . import boards
    if 'sproHal' not in names:
        return None, []
    b = boards.load(board_name or PROJECT['targets'][target]['default_board'])
    boards.check_target(b, target)
    print('board %s (hal %s)' % (b['name'], b['hal']))
    return b, boards.sources(b)


def scode(kit_names, path, refsize, test=False, test_kits=None):
    root = ET.Element('sedonaCode', endian='little', blockSize='4', refSize=str(refsize),
                      main='sys::Sys.main', debug='true', test='true' if test else 'false')
    for n in kit_names:
        el = ET.SubElement(root, 'depend', on=n + ' 1.0+')
        if test:
            el.set('test', str(test_kits is None or n in test_kits).lower())
    ET.indent(root)
    ET.ElementTree(root).write(path, encoding='unicode')
    sedonac(path)
    return path.with_suffix('.scode')


def admin_cred(password, user='admin'):
    return base64.b64encode(hashlib.sha1((user + ':' + password).encode()).digest()).decode()


def app(sax, out, password, fragments=(), kit_names=None):
    """Compile an app: sax with @ADMIN_CRED@ replaced, optional fragments
    (<fragment><kit/><into path=".."><comp/></into><link/></fragment>) merged in,
    and kits not in kit_names dropped from the schema."""
    tree = ET.ElementTree(ET.fromstring(Path(sax).read_text().replace('@ADMIN_CRED@', admin_cred(password))))
    root = tree.getroot()
    schema, app_el, links = root.find('schema'), root.find('app'), root.find('links')
    have = {k.get('name') for k in schema.findall('kit')}
    next_id = 1 + max(int(c.get('id')) for c in app_el.iter('comp'))

    def find(path):
        el = app_el
        for n in [x for x in path.split('/') if x]:
            el = next(c for c in el.findall('comp') if c.get('name') == n)
        return el
    for frag in fragments:
        f = ET.parse(frag).getroot()
        for k in f.findall('kit'):
            if k.get('name') not in have:
                ET.SubElement(schema, 'kit', name=k.get('name'))
                have.add(k.get('name'))
        for into in f.findall('into'):
            target = find(into.get('path', ''))
            for c in into.findall('comp'):
                c = copy.deepcopy(c)
                for x in c.iter('comp'):
                    x.set('id', str(next_id))
                    next_id += 1
                target.append(c)
        for ln in f.findall('link'):
            links.append(copy.deepcopy(ln))
    out.parent.mkdir(parents=True, exist_ok=True)
    tree.write(out, encoding='unicode')
    sedonac(out)
    return out.with_suffix('.sab')


def fragments(folder, names):
    """App fragments in folder whose kits are all in names."""
    have = set(names)
    out = []
    for f in sorted(Path(folder).glob('*.xml')):
        kits = {k.get('name') for k in ET.parse(f).getroot().findall('kit')}
        if kits <= have:
            out.append(f)
    return out


def replaced(frags):
    """frags without the default-app fragments that an example replaces
    (<module replaces="hal">: the example uses the same I/O channels)."""
    drop = set()
    for f in frags:
        drop |= set(split_names(ET.parse(f).getroot().get('replaces', '')))
    if drop:
        print('fragments replaced by examples:', ', '.join(sorted(drop)))
    return [f for f in frags if f.stem not in drop or f.parent.name == 'examples']


def split_names(s):
    return [x.strip() for x in s.split(',') if x.strip()]


def examples(names, kit_names):
    """Fragments examples/<name>.xml; their kits must be in the image."""
    out = []
    for n in names:
        f = ROOT / 'examples' / (n + '.xml')
        if not f.exists():
            raise SystemExit('no example %s (examples/: %s)' % (
                n, ', '.join(p.stem for p in sorted((ROOT / 'examples').glob('*.xml')))))
        need = {k.get('name') for k in ET.parse(f).getroot().findall('kit')} - set(kit_names)
        if need:
            raise SystemExit('example %s needs kits %s: --add %s' % (n, ', '.join(sorted(need)),
                                                                  ','.join(sorted(need))))
        out.append(f)
    return out


def scode_kits(path):
    """[(name, checksum, version)] of the kits linked into a .scode image."""
    b = Path(path).read_bytes()
    if struct.unpack_from('<I', b, 0)[0] != 0x5ED0BA07:
        raise SystemExit('%s: not a little-endian scode image' % path)
    block = b[6]

    def u2(off):
        return struct.unpack_from('<H', b, off)[0]

    def cstr(index):
        off = index * block
        return b[off:b.index(0, off)].decode()
    kits_at, count = u2(20) * block, b[22]
    out = []
    for i in range(count):
        k = u2(kits_at + 2 * i) * block
        out.append((cstr(u2(k + 2)), struct.unpack_from('<I', b, k + 8)[0], cstr(u2(k + 4))))
    return out


def manifests(scode_path, data, mode='all'):
    """<data>/manifests: manifests of exactly the kits in scode_path, as served over
    SOX (m:m.zip, m:<kit>-<checksum>.xml) so an editor with an empty cache can
    load the schema from the device."""
    if mode == 'none':
        return []
    out = data / 'manifests'
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    parts, missing = [], []
    for name, chk, _ in scode_kits(scode_path):
        part = '%s-%08x' % (name, chk)
        src = HOME / 'manifests' / name / (part + '.xml')
        if not src.exists():
            missing.append(part)
            continue
        parts.append((name, part, src.read_bytes()))
    if missing:
        raise SystemExit('no manifest for kit(s) in %s: %s' % (scode_path, ', '.join(missing)))
    if mode in ('all', 'xml'):
        for name, part, xml in parts:
            (out / name).mkdir(exist_ok=True)
            (out / name / (part + '.xml')).write_bytes(xml)
    if mode in ('all', 'zip'):
        with zipfile.ZipFile(out / 'm.zip', 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for name, part, xml in parts:
                info = zipfile.ZipInfo('%s/%s.xml' % (name, part), (2020, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, xml)
    print('manifests (%s): %d kits' % (mode, len(parts)))
    # widgets of the kits (docs/widgets.md), served as m:ui.zip
    from . import ui
    image = scode_kits(scode_path)
    ui.pack([n for n, _, _ in image], image, out / 'ui.zip')
    ui.install()
    return [p for _, p, _ in parts]
