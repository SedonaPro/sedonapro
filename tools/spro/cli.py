# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""spro: build, run and test SedonaPro.

Targets: linux (Linux controller), sim (ESP32 firmware code on this PC), esp32.

  spro kits                          kits by domain, with targets
  spro profiles                      profiles and their kits
  spro packs                         kit sets for a trade (control, pumps ...), --pack NAME
  spro build linux|sim|esp32 [--profile P] [--add k,k] [--remove k,k]
  spro test  linux|sim [--quick]     linux: smoke test (start, web, I/O) + Sedona unit tests
                                     sim: unit tests + platform, SOX, watchdog, safeboot ...
  spro test  install                 install.sh and .deb in a Debian+systemd container (Docker)
  spro run   linux|sim [--erase]     start the VM here (Ctrl+C stops)
  spro package linux [--arch] [--board]   tar.gz with install.sh + .deb -> build/packages
  spro docker  linux [--board]            Docker image sedonapro/svm:<version>-<board>
  spro fw    esp32 [--fw qemu|hw|tests] [--docker]   ESP-IDF firmware build
  spro qemu  esp32 [--fw qemu|tests]  [--docker]   run the firmware in Espressif QEMU
  spro test  esp32                    firmware in QEMU (Docker): boot, SOX, sproHal, save
  spro kit-compile [kit ...]         compile kits (default: all)
  spro boards                        board files (boards/*.toml)
  spro board new NAME --from BOARD   start a board file from an existing one
  spro broker [--port 1883]          small MQTT broker for experiments (logs every message)
  spro pub TOPIC VALUE [--retain]    publish one MQTT message to it
  spro catalog [--check]            docs/kits.md: kits, profiles, components
  spro ui check                      widgets shipped with kits (docs/widgets.md)
  spro editor build|run|test         web editor (editor/): Node 20.19+, JDK 17+; run --writes enables writes
  spro kit new NAME --domain D [--vendor V] [--native]   start a kit from a template

--pack NAME[,NAME]: add the kits of profiles/packs/<name>.toml (docs/pro-kits.md)
--example NAME[,NAME]: add examples/<name>.xml to the app (examples/README.md)
--board NAME: channel table for sproHal (default: linux/sim -> sim, esp32 -> esp32-s3-devkitc)

linux options: --arch amd64|i386|arm64|armhf, --cc, --fast, --asan, --app <targets/linux/apps/NAME.sax>
Default profiles: linux -> pro (full + pro kits), sim/esp32 -> lite; --profile full: classic set.
"""
import argparse
import os
import subprocess
import sys

from . import home as H
from . import project as P


def cmd_kits(args):
    known = P.kits()
    by = {}
    for k in known.values():
        by.setdefault(k.domain, []).append(k)
    for dom in sorted(by):
        print('[%s]' % dom)
        for k in sorted(by[dom], key=lambda k: k.name):
            print('  %-20s %-17s %-7s %s' % (k.name, '+'.join(k.targets),
                                             'native' if k.natives else '', k.summary))


def cmd_profiles(args):
    for p in sorted((P.ROOT / 'profiles').glob('*.toml')):
        prof = P.load_toml(p)
        print('%s: %s\n  %s' % (p.stem, prof.get('summary', ''), ' '.join(prof['kits'])))


def cmd_packs(args):
    for name, pk in P.packs().items():
        print('%s: %s\n  %s' % (name, pk.get('summary', ''), ' '.join(pk['kits'])))


def split(s):
    return [x.strip() for x in (s or '').split(',') if x.strip()]


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    extra = []
    if '--' in argv:
        extra, argv = argv[argv.index('--') + 1:], argv[:argv.index('--')]
    p = argparse.ArgumentParser(prog='spro', description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('command', choices=['kits', 'profiles', 'packs', 'build', 'test', 'run', 'fw', 'qemu',
                                       'kit-compile', 'boards', 'board', 'kit', 'catalog', 'ui', 'broker', 'pub', 'package', 'docker',
                                       'editor'])
    p.add_argument('target', nargs='*')
    p.add_argument('--profile')
    p.add_argument('--add', help='extra kits, comma separated')
    p.add_argument('--pack', help='kit sets from profiles/packs, comma separated')
    p.add_argument('--remove', help='kits to leave out, comma separated')
    p.add_argument('--manifests', choices=['all', 'zip', 'xml', 'none'], default='all',
                   help='kit manifests in the image for SOX clients (default all)')
    p.add_argument('--arch', default='amd64')
    p.add_argument('--fast', action='store_true', help='linux: -O3 and computed-goto dispatch')
    p.add_argument('--asan', action='store_true', help='linux: AddressSanitizer build (tests only)')
    p.add_argument('--cc', default=os.environ.get('CC', 'gcc'))
    p.add_argument('--app', default='defaultApp')
    p.add_argument('--tests', action='store_true', help='esp32: put tests.scode into the data')
    p.add_argument('--erase', action='store_true', help='run sim: start from a fresh flash')
    p.add_argument('--fw', default='qemu', choices=['qemu', 'hw', 'tests'],
                   help='firmware config: targets/esp32/firmware/sdkconfig.<fw>')
    p.add_argument('--docker', action='store_true', help='ESP-IDF from the %s image' % 'espressif/idf')
    p.add_argument('-k', help='test sim: only tests whose name contains this')
    p.add_argument('--board', help='boards/<name>.toml for sproHal')
    p.add_argument('--example', help='add examples/<name>.xml to the app, comma separated')
    p.add_argument('--quick', action='store_true', help='test linux: smoke test only, no unit tests')
    p.add_argument('--stages', action='store_true',
                   help='test linux: also the VM runtime checks (stage, rollback, watchdog, Modbus; ~6 min, '
                        'ASan checks need ./spro build linux --asan)')
    p.add_argument('--from', dest='template', default='sim', help='board new: template board')
    p.add_argument('--domain', help='kit new: kits/<domain>/ (io, comm, logic, data, ui ...)')
    p.add_argument('--vendor', default='SPro', help='kit new: vendor, the kit name starts with it')
    p.add_argument('--port', type=int, default=1883, help='broker, pub: MQTT port')
    p.add_argument('--host', default='127.0.0.1', help='pub: MQTT broker')
    p.add_argument('--retain', action='store_true', help='pub: retained message')
    p.add_argument('--check', action='store_true', help='catalog: fail if docs/kits.md is out of date')
    p.add_argument('--native', action='store_true', help='kit new: add a C native and a native id')
    p.add_argument('--http-port', type=int, default=8098, help='editor run: HTTP port on 127.0.0.1')
    p.add_argument('--writes', action='store_true', help='editor run: allow device writes and actions')
    a = p.parse_args(argv)
    if a.command == 'kits':
        return cmd_kits(a)
    if a.command == 'profiles':
        return cmd_profiles(a)
    if a.command == 'packs':
        return cmd_packs(a)
    if a.command == 'kit-compile':
        return H.compile_kits(a.target or None)
    if a.command in ('boards', 'board'):
        from . import boards as B
        if a.command == 'boards':
            return B.describe()
        if len(a.target) != 2 or a.target[0] != 'new':
            sys.exit('usage: spro board new NAME --from BOARD')
        return B.new(a.target[1], a.template)
    if a.command in ('broker', 'pub'):
        broker = [sys.executable, str(P.ROOT / 'tools/mqtt_broker.py')]
        if a.command == 'broker':
            print('MQTT broker on 127.0.0.1:%d, Ctrl+C to stop' % a.port, flush=True)
            try:
                return subprocess.run(broker + [str(a.port)]).returncode
            except KeyboardInterrupt:
                return 0
        if len(a.target) != 2:
            sys.exit('usage: spro pub TOPIC VALUE [--retain] [--host H] [--port P]')
        return subprocess.run(broker + ['pub', a.host, str(a.port), a.target[0], a.target[1]] +
                              (['--retain'] if a.retain else [])).returncode
    if a.command == 'editor':
        from . import editor as ED
        if a.target == ['build']:
            return ED.build()
        if a.target == ['run']:
            return ED.run(a.http_port, a.writes)
        if a.target == ['test']:
            return ED.test()
        sys.exit('usage: spro editor build | run [--http-port N] [--writes] | test')
    if a.command == 'ui':
        from . import ui
        if a.target != ['check']:
            sys.exit('usage: spro ui check')
        return ui.cmd_check()
    if a.command == 'catalog':
        from . import catalog
        return catalog.catalog(a.check)
    if a.command == 'kit':
        from . import newkit
        if len(a.target) != 2 or a.target[0] != 'new' or not a.domain:
            sys.exit('usage: spro kit new NAME --domain D [--vendor V] [--native]')
        return newkit.new(a.target[1], a.domain, a.vendor, a.native)
    target = (a.target or ['linux'])[0]
    if a.command == 'test' and target == 'install':
        pk = H.BUILD / 'packages'
        if not list(pk.glob('*.deb')):
            sys.exit('first: ./spro package linux')
        return subprocess.run(['sh', str(P.ROOT / 'targets/linux/tests/install_test.sh'), str(pk)]).returncode
    if target not in P.TARGETS:
        sys.exit('unknown target %s (%s)' % (target, ', '.join(P.TARGETS)))
    add, remove = split(a.add), split(a.remove)
    add += [k for k in P.pack_kits(split(a.pack)) if k not in add]

    if target == 'linux':
        from . import linux as L
        if a.command == 'build':
            L.build(a.arch, a.profile, add, remove, a.fast, a.asan, a.cc, a.app, manifests=a.manifests,
                    board=a.board, examples=split(a.example))
        elif a.command == 'test':
            run = L.outdir(a.arch, a.profile, a.board) / 'run'
            smoke = subprocess.run([sys.executable, str(P.ROOT / 'targets/linux/tests/smoke_test.py'),
                                    str(run)]).returncode
            if a.stages:
                for n in (1, 2, 3):
                    rc = subprocess.run([sys.executable, str(P.ROOT / ('targets/linux/tests/svm_stage%d_test.py' % n)),
                                         '--arch', a.arch]).returncode
                    smoke = smoke or rc
            if a.quick:
                return smoke
            return 0 if L.unit_tests(a.arch, a.profile, board=a.board) and smoke == 0 else 1
        elif a.command in ('package', 'docker'):
            from . import package as K
            if a.command == 'package':
                K.package(a.arch, a.profile, a.board)
            else:
                K.docker_image(a.arch, a.profile, a.board)
        elif a.command == 'run':
            run = L.outdir(a.arch, a.profile, a.board) / 'run'
            os.chdir(run)
            os.execv(str(run / 'svm'), [str(run / 'svm'), '--plat', *extra])
        else:
            sys.exit('%s: not for target linux' % a.command)
        return 0

    from . import esp32 as E
    if a.command == 'build':
        if target == 'sim':
            E.build_sim(a.profile, add, remove, manifests=a.manifests, board=a.board,
                        examples=split(a.example), arch=a.arch, cc=a.cc)
        else:
            E.build_esp32(a.profile, add, remove, manifests=a.manifests,
                          tests=a.tests or a.fw == 'tests', board=a.board,
                          examples=split(a.example))
    elif a.command == 'run' and target == 'sim':
        E.run_sim(a.erase, extra)
    elif a.command == 'test' and target == 'sim':
        import runpy
        sys.argv = ['sim_test.py'] + (['-k', a.k] if a.k else [])
        try:
            runpy.run_path(str(P.ROOT / 'targets/esp32/tests/sim_test.py'), run_name='__main__')
        except SystemExit as e:
            return e.code
    elif a.command == 'test' and target == 'esp32':
        return subprocess.run([sys.executable, str(P.ROOT / 'targets/esp32/tests/qemu_test.py')]).returncode
    elif a.command == 'fw' and target == 'esp32':
        E.idf(a.fw, ['build'], a.docker)
    elif a.command == 'qemu' and target == 'esp32':
        args = ['qemu']
        if a.fw == 'qemu':
            args += ['--qemu-extra-args', '-nic user,model=open_eth,hostfwd=udp::1876-:1876']
        # serial console straight to the terminal (idf.py monitor needs a pty in the container)
        print('QEMU: SOX on udp/127.0.0.1:1876, Ctrl+C to stop')
        E.idf(a.fw, args, a.docker)
    else:
        sys.exit('%s: not for target %s' % (a.command, target))
    return 0
