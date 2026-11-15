# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Project model: project.toml, kits (kit.xml + kit.toml), profiles, targets.

A kit is any directory with a kit.xml under core/kits, kits/ or platforms/.
Its kit.toml (optional, defaults below) says where it builds and which native
sources it brings:

    summary = "Modbus TCP/RTU master"
    targets = ["linux", "sim"]            # default: all targets
    [native]                              # dirs relative to the kit
    all   = ["native"]                    # default: ["native"] if it exists
    linux = ["native/linux"]
    [build]
    cflags = ["-I/usr/include/modbus"]    # host builds (linux, sim)
    libs   = ["-lmodbus"]

A profile (profiles/<name>.toml) is the list of kits that go into kits.scode;
their kit.xml dependencies are added automatically.  A pack
(profiles/packs/<name>.toml) is a set of kits for one trade (control, pumps,
hvac ...) added to any profile with --pack.

    portable_tests = true                 # kit.toml: unit tests are plain Sedona,
                                          # they also run in the ESP32 simulator
    test_only = true                      # only in tests.scode, added when all its
                                          # dependencies are in the image
"""
from dataclasses import dataclass, field
from pathlib import Path
import tomllib
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
TARGETS = ('linux', 'sim', 'esp32')
KIT_ROOTS = ('core/kits', 'kits', 'platforms')


def load_toml(path):
    with open(path, 'rb') as f:
        return tomllib.load(f)


PROJECT = load_toml(ROOT / 'project.toml')


@dataclass
class Kit:
    name: str
    dir: Path
    depends: list
    natives: bool
    summary: str = ''
    targets: tuple = TARGETS
    native_dirs: dict = field(default_factory=dict)
    cflags: list = field(default_factory=list)
    libs: list = field(default_factory=list)
    portable_tests: bool = False
    test_only: bool = False

    @property
    def rel(self):
        return self.dir.relative_to(ROOT)

    @property
    def domain(self):
        parts = self.rel.parts
        return 'core' if parts[0] == 'core' else 'platform' if parts[0] == 'platforms' else parts[1]

    def sources(self, target):
        """Native source dirs (sedona.home paths) for target."""
        dirs = self.native_dirs.get('all', []) + self.native_dirs.get(target, [])
        return ['/repo/%s/%s' % (self.rel.as_posix(), d) for d in dirs]


def _load_kit(xml_path):
    root = ET.parse(xml_path).getroot()
    if root.tag != 'sedonaKit':
        return None
    d = xml_path.parent
    meta = load_toml(d / 'kit.toml') if (d / 'kit.toml').exists() else {}
    natives = root.find('native') is not None
    native_dirs = dict(meta.get('native', {}))
    if 'all' not in native_dirs:
        native_dirs['all'] = ['native'] if natives and (d / 'native').is_dir() else []
    build = meta.get('build', {})
    return Kit(name=root.get('name'), dir=d,
               depends=[e.get('on').split()[0] for e in root.findall('depend')],
               natives=natives, summary=meta.get('summary', root.get('description', '')),
               targets=tuple(meta.get('targets', TARGETS)), native_dirs=native_dirs,
               cflags=build.get('cflags', []), libs=build.get('libs', []),
               portable_tests=bool(meta.get('portable_tests', False)),
               test_only=bool(meta.get('test_only', False)))


def kits():
    found = {}
    for base in KIT_ROOTS:
        for p in sorted((ROOT / base).rglob('kit.xml')):
            k = _load_kit(p)
            if k is None:
                continue
            if k.name in found:
                raise SystemExit('kit %s defined twice: %s and %s' % (k.name, found[k.name].rel, k.rel))
            found[k.name] = k
    return found


def closure(names, all_kits):
    """names plus their kit.xml dependencies, in dependency order."""
    out, visiting = [], set()

    def visit(n):
        if n in out:
            return
        if n not in all_kits:
            raise SystemExit('unknown kit: %s' % n)
        if n in visiting:
            raise SystemExit('cyclic kit dependency at %s' % n)
        visiting.add(n)
        for d in all_kits[n].depends:
            visit(d)
        visiting.discard(n)
        out.append(n)
    for n in names:
        visit(n)
    return out


def profile(name):
    path = ROOT / 'profiles' / (name + '.toml')
    if not path.exists():
        raise SystemExit('no profile %s (profiles/*.toml: %s)' % (
            name, ', '.join(p.stem for p in sorted((ROOT / 'profiles').glob('*.toml')))))
    return load_toml(path)


def test_extras(names, all_kits):
    """test_only kits whose dependencies are all in names."""
    have = set(names)
    out = []
    for k in sorted(all_kits.values(), key=lambda k: k.name):
        if k.test_only and k.name not in have and set(closure([k.name], all_kits)) - {k.name} <= have:
            out.append(k.name)
    return out


def packs():
    """{name: pack toml} of profiles/packs/*.toml"""
    return {p.stem: load_toml(p) for p in sorted((ROOT / 'profiles/packs').glob('*.toml'))}


def pack_kits(names):
    """Kits of the named packs, in order, without repeats."""
    all_packs = packs()
    out = []
    for n in names:
        if n not in all_packs:
            raise SystemExit('no pack %s (profiles/packs: %s)' % (n, ', '.join(all_packs) or '-'))
        out += [k for k in all_packs[n]['kits'] if k not in out]
    return out


def select(profile_name, target, add=(), remove=()):
    """Kits of a profile for a target: (ordered kit names, all kits)."""
    all_kits = kits()
    prof = profile(profile_name)
    names = [n for n in prof['kits'] if n not in remove] + [n for n in add if n not in prof['kits']]
    plat = PROJECT['targets'][target]['platform_kit']
    if plat not in names:
        names.append(plat)
    ordered = closure(names, all_kits)
    bad = [n for n in ordered if target not in all_kits[n].targets]
    if bad:
        raise SystemExit('profile %s: kit(s) not available for target %s: %s' % (
            profile_name, target, ', '.join(bad)))
    return ordered, all_kits
