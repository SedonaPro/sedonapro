# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Targets sim and esp32: the ESP32 firmware's Sedona side.

sim    the firmware's C code (VM, natives, platforms/esp32) built as a Linux
       program with ESP-IDF shims from emu/esp32sim; refSize 8 (64-bit host).
       build/sim/{native, data, esp32sim, flash}
esp32  natives + LittleFS data for the ESP-IDF project in targets/esp32/firmware;
       refSize 4.  build/esp32/{native, data, fw-<profile>}
       The firmware is built with ESP-IDF from the environment (IDF_PATH) or,
       with --docker, in the espressif/idf image.

The demo app is targets/esp32/apps/esp32-demo.sax plus every fragment in
targets/esp32/apps/fragments whose kits are all in the image.
"""
import os
import pwd
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

from . import home as H
from .project import ROOT, PROJECT, select, test_extras, kits as all_kits

T = ROOT / 'targets/esp32'
FW = T / 'firmware'
DEV_PASSWORD = 'sedonapro-dev'
IDF_IMAGE = 'espressif/idf:v6.0.3'
# file functions the simulator redirects into its --flash directory
WRAPPED = ['fopen', 'rename', 'remove', 'unlink', 'stat', 'open', 'mkdir', 'chdir']
# kits whose own tests run in tests.scode (the rest need network or hardware)
TEST_KITS = {'sys', 'types', 'math', 'sproMath', 'datetime', 'func', 'logic', 'sproHal', 'sproMqtt'}


def outdir(target):
    return H.BUILD / target


def fragments(names):
    return H.fragments(T / 'apps/fragments', names)


def data_dir(target, names, refsize, out, password, manifests, tests, examples=()):
    data = out / 'data'
    if data.exists():
        shutil.rmtree(data)
    data.mkdir(parents=True)
    shutil.copy2(H.scode(names, out / 'kits.xml', refsize), data / 'kits.scode')
    if tests:
        known = all_kits()
        extras = test_extras(names, known)
        H.compile_kits(extras)
        test_kits = TEST_KITS | {n for n in names + extras if known[n].portable_tests}
        shutil.copy2(H.scode(names + extras, out / 'tests.xml', refsize, test=True, test_kits=test_kits),
                     data / 'tests.scode')
    frags = H.replaced(fragments(names) + H.examples(examples, names))
    print('app fragments:', ', '.join(f.stem for f in frags) or '-')
    shutil.copy2(H.app(T / 'apps/esp32-demo.sax', out / 'app.sax', password, frags), data / 'app.sab')
    shutil.copy2(FW / 'svm.conf', data / 'svm.conf')
    H.manifests(data / 'kits.scode', data, manifests)
    (out / 'kits.txt').write_text('\n'.join(names) + '\n')
    for f in sorted(p for p in data.iterdir() if p.is_file()):
        print('  data/%-12s %7d bytes' % (f.name, f.stat().st_size))
    return data


def prepare(target, profile, add, remove, refsize, board=None):
    profile = profile or PROJECT['targets'][target]['default_profile']
    names, known = select(profile, target, add, remove)
    print('%s profile %s: %s' % (target, profile, ' '.join(names)))
    out = outdir(target)
    out.mkdir(parents=True, exist_ok=True)
    H.compile_kits(names)
    plat_id = '%s-%s-%s' % (PROJECT['prefix'], 'esp32s3' if target == 'esp32' else 'esp32sim',
                            PROJECT['version'])
    brd, extra = H.board_for(target, names, board)
    manifest = H.platform_manifest(target, names, known, out, refsize, plat_id, extra)
    H.natives(manifest, out / 'native', brd)
    return names, known, out


def password_or_default(password):
    password = password or os.environ.get('SEDONA_ADMIN_PASSWORD')
    if not password:
        print('NOTE: admin password is the development default %r (set SEDONA_ADMIN_PASSWORD)'
              % DEV_PASSWORD)
    return password or DEV_PASSWORD


def build_sim(profile=None, add=(), remove=(), password=None, manifests='all', board=None, examples=(),
              arch='amd64', cc='gcc'):
    """arch i386 (with a 32-bit cc, e.g. emu/i386/cc): refSize 4 like the chip."""
    refsize = 4 if arch in ('i386', 'armhf') else 8
    names, known, out = prepare('sim', profile, add, remove, refsize, board)
    native = out / 'native'
    srcs = [p for p in sorted(native.glob('*.c')) if p.name not in ('svm_crash.c', 'svm_rt.c', 'main.c')]
    version = H.sedona_version()
    flags = ['-std=gnu99', '-O2', '-g', '-fno-strict-aliasing', '-fcommon', '-D_GNU_SOURCE',
             '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=2', '-DSEDONA_LOCAL_H', '-DESP_PLATFORM=1', '-DESP32SIM=1', '-DSOCKET_FAMILY_INET=1',
             '-DPLAT_BUILD_VERSION="%s-esp32sim"' % version, '-DCOMPUTED_GOTO',
             '-I' + str(ROOT / 'emu/esp32sim/include'), '-I' + str(native)]
    libs = []
    for n in names:
        flags += [f for f in known[n].cflags if f not in flags]
        libs += [l for l in known[n].libs if l not in libs]
    objs = out / 'obj'
    if objs.exists():
        shutil.rmtree(objs)
    objs.mkdir()
    H.run([cc, *flags, '-Dmain=svm_main', '-c', native / 'main.c', '-o', objs / 'svm_main.o'])
    wraps = ['-Wl,--wrap=' + f for f in WRAPPED + ['malloc', 'calloc', 'realloc']]
    tmp = out / 'esp32sim.new'
    H.run([cc, *flags, *srcs, ROOT / 'emu/esp32sim/esp32sim.c', objs / 'svm_main.o',
           '-o', tmp, *wraps, *libs, '-lm', '-pthread', '-ldl'])
    tmp.replace(out / 'esp32sim')
    data_dir('sim', names, refsize, out, password_or_default(password), manifests, tests=True, examples=examples)
    print('BUILT', out / 'esp32sim')


def run_sim(erase=False, extra=()):
    out = outdir('sim')
    flash = out / 'flash'
    if erase and flash.exists():
        shutil.rmtree(flash)
    if not flash.exists():
        shutil.copytree(out / 'data', flash)
        print('flashed', flash)
    os.execv(str(out / 'esp32sim'), [str(out / 'esp32sim'), '--flash', str(flash), *extra])


def build_esp32(profile=None, add=(), remove=(), password=None, manifests='all', tests=False,
                board=None, examples=()):
    names, known, out = prepare('esp32', profile, add, remove, 4, board)
    data_dir('esp32', names, 4, out, password_or_default(password), manifests, tests, examples)
    print('BUILT', out / 'data', '(firmware: spro fw esp32)')


def idf(fw_profile, args, docker=False):
    """idf.py in targets/esp32/firmware with sdkconfig.defaults + sdkconfig.<fw_profile>."""
    out = outdir('esp32') / ('fw-' + fw_profile)
    cmd = ['idf.py', '-C', FW, '-B', out, '-DSDKCONFIG=' + str(out / 'sdkconfig'),
           '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.' + fw_profile, *args]
    if docker:
        # same absolute paths inside the container, files owned by the caller
        cmd = ['docker', 'run', '--rm', '-t', *(['-i'] if sys.stdin.isatty() else []), '-v', '%s:%s' % (ROOT, ROOT), '-w', str(ROOT),
               '-u', '%d:%d' % (os.getuid(), pwd.getpwuid(os.getuid()).pw_gid), '-e', 'HOME=/tmp',
               *(['--network', 'host'] if 'qemu' in args else []), IDF_IMAGE, *map(str, cmd)]
    elif not os.environ.get('IDF_PATH'):
        sys.exit('ESP-IDF is not in the environment (. ~/esp/esp-idf/export.sh), or use --docker')
    H.run(cmd)
