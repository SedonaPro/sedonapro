# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Target linux: svm for a Linux controller and its run directory.

build/linux-<arch>-<profile>-<board>/
    native/      staged C sources (sedonac)
    run/         what goes to the controller: svm, kits.scode, app.sab, svm.conf, manifests/
    tests.scode  unit tests of the profile's kits (tests-1.scode, tests-2.scode ...
                 when they do not fit into one image)
"""
import os
import shutil
import subprocess

from . import home as H
from .project import ROOT, PROJECT, select, test_extras

ARCHS = {  # arch -> (refSize, gcc flags)
    'amd64': (8, ['-m64']),
    'i386': (4, ['-m32']),
    'arm64': (8, []),
    'armhf': (4, []),
}
DEV_PASSWORD = 'sedonapro-dev'


def outdir(arch, profile=None, board=None):
    profile = profile or PROJECT['targets']['linux']['default_profile']
    board = board or PROJECT['targets']['linux']['default_board']
    return H.BUILD / ('linux-%s-%s-%s' % (arch, profile, board))


def build(arch='amd64', profile=None, add=(), remove=(), fast=False, asan=False, cc='gcc',
          app='defaultApp', password=None, manifests='all', board=None, examples=()):
    profile = profile or PROJECT['targets']['linux']['default_profile']
    names, known = select(profile, 'linux', add, remove)
    print('linux/%s profile %s: %s' % (arch, profile, ' '.join(names)))
    out = outdir(arch, profile, board)
    run = out / 'run'
    run.mkdir(parents=True, exist_ok=True)
    H.compile_kits(names)
    refsize, archflags = ARCHS[arch]
    version = H.sedona_version()
    plat_id = '%s-linux-%s-%s' % (PROJECT['prefix'], arch, PROJECT['version'])
    brd, extra = H.board_for('linux', names, board)
    manifest = H.platform_manifest('linux', names, known, out, refsize, plat_id, extra)
    native = out / 'native'
    H.natives(manifest, native, brd)

    flags = [*archflags, '-std=gnu99', '-O2', '-g', '-fno-strict-aliasing', '-fcommon',
             '-D_GNU_SOURCE', '-D__UNIX__=1', '-DSOCKET_FAMILY_INET=1',
             '-DPLAT_BUILD_VERSION="%s-%s"' % (version, PROJECT['prefix']), '-I' + str(native)]
    libs = []
    for n in names:
        flags += [f for f in known[n].cflags if f not in flags]
        libs += [l for l in known[n].libs if l not in libs]
    if fast:
        flags = ['-O3' if f == '-O2' else f for f in flags] + ['-DCOMPUTED_GOTO']
    if asan:
        flags = ['-O1' if f in ('-O2', '-O3') else f for f in flags] + \
                ['-fsanitize=address', '-fno-omit-frame-pointer']
    else:
        # buffer overflow checks of libc string/printf calls (Ubuntu's gcc does this by default)
        flags += ['-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=2']
    tmp = out / 'svm.new'
    H.run([cc, *flags, *sorted(native.glob('*.c')), '-o', tmp, *libs, '-lm', '-pthread',
           *(['-fsanitize=address'] if asan else [])])
    tmp.replace(run / ('svm.asan' if asan else 'svm'))

    kits_scode = H.scode(names, out / 'kits.xml', refsize)
    shutil.copy2(kits_scode, run / 'kits.scode')
    extras = test_extras(names, known)
    H.compile_kits(extras)
    test_images(names + extras, out, refsize)
    password = password or os.environ.get('SEDONA_ADMIN_PASSWORD')
    if not password:
        password = DEV_PASSWORD
        print('NOTE: admin password is the development default %r (set SEDONA_ADMIN_PASSWORD)'
              % DEV_PASSWORD)
    sax = ROOT / 'targets/linux/apps' / (app + '.sax')
    frags = H.fragments(ROOT / 'targets/linux/apps/fragments', names) if app == 'defaultApp' else []
    frags = H.replaced(frags + H.examples(examples, names))
    print('app %s, fragments: %s' % (app, ', '.join(f.stem for f in frags) or '-'))
    shutil.copy2(H.app(sax, out / 'app.sax', password, frags), run / 'app.sab')
    conf = ROOT / 'targets/linux/svm.conf.example'
    if not (run / 'svm.conf').exists():
        shutil.copy2(conf, run / 'svm.conf')
    H.manifests(run / 'kits.scode', run, manifests)
    platdb = H.HOME / 'platforms/db' / PROJECT['prefix'] / 'linux' / arch / PROJECT['version']
    platdb.mkdir(parents=True, exist_ok=True)
    shutil.copy2(native / '.par/platformManifest.xml', platdb / 'platformManifest.xml')
    print('BUILT', run)
    return out


def test_images(names, out, refsize):
    """tests.scode with the tests of all kits; if the image would pass the
    16-bit block limit of the VM, the kits with tests are split in halves
    into tests-1.scode, tests-2.scode ..."""
    for old in out.glob('tests*.scode'):
        old.unlink()
    for old in out.glob('tests-*.xml'):
        old.unlink()

    def fits(path, group):
        try:
            H.scode(names, path, refsize, test=True, test_kits=set(group))
            return True
        except subprocess.CalledProcessError:
            return False
    if fits(out / 'tests.xml', names):
        return [out / 'tests.scode']
    groups, todo = [], [list(names)]
    while todo:
        g = todo.pop(0)
        path = out / ('tests-%d.xml' % (len(groups) + 1))
        if fits(path, g):
            groups.append(path.with_suffix('.scode'))
        elif len(g) == 1:
            raise SystemExit('tests of kit %s alone do not fit into an image' % g[0])
        else:
            todo[:0] = [g[:len(g) // 2], g[len(g) // 2:]]
    print('unit tests split into %d images' % len(groups))
    return groups


def unit_tests(arch='amd64', profile=None, timeout=600, board=None):
    """Run the Sedona unit tests of every kit in the profile on the built svm."""
    profile = profile or PROJECT['targets']['linux']['default_profile']
    out = outdir(arch, profile, board)
    images = sorted(out.glob('tests*.scode'))
    if not images:
        raise SystemExit('build first: spro build linux --arch %s --profile %s --board %s' % (arch, profile, board))
    log = out / 'unit-tests.log'
    ok, asserts = True, 0
    with log.open('w') as f:
        for img in images:
            f.write('=== %s\n' % img.name)
            f.flush()
            rc = subprocess.run([str(out / 'run/svm'), str(img), '-test'], cwd=out, stdout=f,
                                stderr=subprocess.STDOUT, timeout=timeout).returncode
            ok = ok and rc == 0
    text = log.read_text(errors='replace')
    import re
    fails = sum(int(n) for n in re.findall(r'Assert Failures  = (\d+)', text))
    asserts = sum(int(n) for n in re.findall(r'Assert Successes = (\d+)', text))
    print(text[-600:])
    ok = ok and fails == 0 and len(re.findall(r'Assert Failures', text)) == len(images) \
        and 'WARNING: stack imbalance' not in text
    print('unit tests: %s, %d images, %d asserts, %d failures (log %s)' % (
        'PASS' if ok else 'FAIL', len(images), asserts, fails, log))
    return ok
