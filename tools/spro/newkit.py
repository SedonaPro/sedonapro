# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""spro kit new: start a kit from a working template.

    spro kit new acmeBoiler --domain logic --vendor Acme [--native]

Creates kits/<domain>/<name>/ with kit.xml, kit.toml, README.md, one component
(Gain: out = in * gain, limited to lo..hi), its unit test and, with --native,
a C native method and the next free community native id (registry/native-ids.md).
The kit builds and its test passes as generated:

    ./spro build linux --add <name> && ./spro test linux
"""
import re
import sys
import xml.etree.ElementTree as ET

from . import project as P

COMMUNITY_IDS = range(200, 255)
REGISTRY = P.ROOT / 'registry/native-ids.md'
HEADER = '''//
// Copyright {year} {vendor} contributors
// Licensed under the Apache License, Version 2.0
//
'''


def used_native_ids():
    ids = set()
    for base in P.KIT_ROOTS:
        for x in (P.ROOT / base).rglob('kit.xml'):
            try:
                root = ET.parse(x).getroot()
            except ET.ParseError:
                continue
            for n in root.findall('native'):
                ids.add(int(n.get('id').split('::')[0]))
    ids.update(int(m) for m in re.findall(r'^\| (\d+) \|', REGISTRY.read_text(), re.M))
    return ids


def new(name, domain, vendor='SPro', native=False, year='2026'):
    if not re.fullmatch(r'[a-z][A-Za-z0-9]{1,30}', name):
        sys.exit('kit name: letters and digits, starts with a lowercase letter (acmeBoiler)')
    if not re.fullmatch(r'[A-Z][A-Za-z0-9]{1,15}', vendor):
        sys.exit('vendor: starts with a capital letter (Acme)')
    if not name.lower().startswith(vendor.lower()) or len(name) == len(vendor):
        sys.exit('sedonac rule: kit name must start with the vendor name: %s%s...' % (vendor.lower(), 'Xxx'))
    if not re.fullmatch(r'[a-z][a-z0-9-]*', domain):
        sys.exit('domain: lowercase directory name (io, comm, logic, data, ui ...)')
    if name in P.kits():
        sys.exit('kit %s already exists: %s' % (name, P.kits()[name].rel))
    d = P.ROOT / 'kits' / domain / name
    if d.exists():
        sys.exit('%s exists' % d.relative_to(P.ROOT))
    if not (P.ROOT / 'kits' / domain).is_dir():
        print('new domain kits/%s (existing: %s)' % (domain, ', '.join(
            sorted(p.name for p in (P.ROOT / 'kits').iterdir() if p.is_dir()))))

    kid = None
    if native:
        free = [i for i in COMMUNITY_IDS if i not in used_native_ids()]
        if not free:
            sys.exit('no free community native id in 200-254')
        kid = free[0]

    head = HEADER.format(year=year, vendor='SedonaPro' if vendor == 'SPro' else vendor)
    files = {}
    clamp = ('  ** limits v to lo..hi (C: native/%s_Gain.c)\n'
             '  static native float clamp(float v, float lo, float hi)\n' % name) if native else '''\
  ** limits v to lo..hi
  static float clamp(float v, float lo, float hi)
  {
    if (v < lo) return lo
    if (v > hi) return hi
    return v
  }
'''
    files['Gain.sedona'] = head + '''
**
** Example component: out = in * gain, limited to lo..hi.
** Rename or replace it; every public class of the kit is a palette entry.
**
class Gain extends Component
{
  ** input value
  property float in

  ** multiplier
  @config property float gain = 1.0f

  ** lower limit of out
  @config property float lo = -1000.0f

  ** upper limit of out
  @config property float hi = 1000.0f

  ** in * gain, limited to lo..hi
  @readonly property float out

  override virtual void execute()
  {
    out := clamp(in * gain, lo, hi)
  }

''' + clamp + '}\n'
    files['test/GainTest.sedona'] = head + '''
**
** Unit tests run with: ./spro build linux --add %s && ./spro test linux
**
class GainTest extends Test
{
  static inline Gain g

  static void test()
  {
    g.in := 2.0f
    g.gain := 3.0f
    g.execute()
    assert(g.out == 6.0f)

    g.hi := 5.0f
    g.execute()
    assert(g.out == 5.0f)

    g.in := -2000.0f
    g.gain := 1.0f
    g.execute()
    assert(g.out == -1000.0f)
  }
}
''' % name
    natives = ''
    if native:
        natives = '\n  <native qname="%s::Gain.clamp" id="%d::0" />\n' % (name, kid)
        files['native/%s_Gain.c' % name] = head.replace('//\n// Copyright', '//\n// %s natives.\n// Copyright' % name, 1) + '''
#include "sedona.h"

// float Gain.clamp(float v, float lo, float hi)
Cell %s_Gain_clamp(SedonaVM* vm, Cell* params)
{
  float v = params[0].fval, lo = params[1].fval, hi = params[2].fval;
  Cell r;
  r.fval = v < lo ? lo : v > hi ? hi : v;
  return r;
}
''' % name
    files['kit.xml'] = '''<!--
  -  %(name)s
  -  Licensed under the Apache License, Version 2.0
  -->
<sedonaKit
 name = "%(name)s"
 vendor = "%(vendor)s"
 description = "TODO: one line about the kit"
 includeSource = "false"
 doc = "true"
>
  <depend on="sys 1.2+" />

  <source dir="." />
  <source dir="test" testonly="true" />
%(natives)s</sedonaKit>
''' % dict(name=name, vendor=vendor, natives=natives)
    files['kit.toml'] = '''summary = "TODO: one line for ./spro kits"
# Where the kit builds (default: all targets). Drop esp32 if it needs Linux APIs.
targets = ["linux", "sim", "esp32"]
# Native sources: native/ for every target; add per-target dirs if needed:
# [native]
# linux = ["native/linux"]
# esp32 = ["native/esp32"]
# Host libraries (linux, sim):
# [build]
# cflags = ["-I/usr/include/foo"]
# libs = ["-lfoo"]
'''
    files['README.md'] = '''# %(name)s

TODO: what the kit is for.

| Component | Slots | What it does |
|---|---|---|
| `Gain` | `in`, `gain`, `lo`, `hi`, `out` | out = in * gain, limited to lo..hi |

Build and test:

```sh
./spro build linux --add %(name)s && ./spro test linux
./spro build sim --add %(name)s && ./spro test sim
```
%(native)s''' % dict(name=name, native='\nNative kit id: %d (registry/native-ids.md).\n' % kid if native else '')

    for rel, text in files.items():
        p = d / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    if native:
        reg = REGISTRY.read_text().rstrip('\n') + '\n| %d | %s | — (new) |\n' % (kid, name)
        REGISTRY.write_text(reg)
    print('created %s/' % d.relative_to(P.ROOT))
    for rel in sorted(files):
        print('  ' + rel)
    if native:
        print('native id %d added to registry/native-ids.md' % kid)
    if vendor.lower() != 'spro':
        print('add your prefix to registry/vendors.md')
    print('next: ./spro build linux --add %s && ./spro test linux' % name)
