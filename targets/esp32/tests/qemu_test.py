#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""ESP32-S3 firmware in Espressif QEMU (espressif/idf image): boot, VM running,
SOX over the emulated Ethernet (platform info, sproHal board, write, save).
Needs: ./spro build esp32 && ./spro fw esp32 --fw qemu --docker
usage: qemu_test.py [sox host port]
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
FW = ROOT / 'build/esp32/fw-qemu'
HOME = ROOT / 'build/home'
JAVA = ROOT / 'build/tests-java'
IMAGE = 'espressif/idf:v6.0.3'
NAME = 'spro-qemu-test'
PASSWORD = os.environ.get('SEDONA_ADMIN_PASSWORD', 'sedonapro-dev')


def docker(*args, **kw):
    return subprocess.run(['docker', *args], capture_output=True, text=True, **kw)


def log():
    return re.sub(r'\x1b\[[0-9;]*m', '', docker('logs', NAME).stdout + docker('logs', NAME).stderr)


def sox(port, cmd):
    r = subprocess.run(['java', '-Dsedona.home=' + str(HOME), '-cp',
                        os.pathsep.join([str(HOME / 'lib/sedona.jar'), str(JAVA)]),
                        'SoxProbe', '127.0.0.1', str(port), cmd],
                       env=dict(os.environ, SEDONA_ADMIN_PASSWORD=PASSWORD),
                       capture_output=True, text=True, timeout=90)
    if 'PROBE %s OK' % cmd not in r.stdout:
        raise AssertionError('SOX %s: %s %s' % (cmd, r.stdout, r.stderr[-500:]))
    return r.stdout


def mqtt(port):
    """MQTT over the emulated Ethernet to a broker on this machine (10.0.2.2 in QEMU)."""
    broker_py = ROOT / 'tools/mqtt_broker.py'
    mlog = ROOT / 'build/esp32/qemu-mqtt.log'
    with mlog.open('w') as f:
        broker = subprocess.Popen([sys.executable, str(broker_py), '18832'], stdout=f, stderr=subprocess.STDOUT)
    try:
        time.sleep(0.5)
        for c in ('host=10.0.2.2', 'port=18832', 'enabled=true'):
            sox(port, 'set:/service/mqtt.' + c)
        seen = lambda s: s in mlog.read_text()
        end = time.time() + 30
        while time.time() < end and not seen('SUBSCRIBE sedonapro/esp32/cmd/mode'):
            time.sleep(0.5)
        assert seen('PUBLISHr sedonapro/esp32/status online'), mlog.read_text()
        subprocess.run([sys.executable, str(broker_py), 'pub', '127.0.0.1', '18832',
                        'sedonapro/esp32/cmd/sp', '22.5'], check=True)
        end = time.time() + 15
        while time.time() < end and not seen('"sp":22.5}'):
            time.sleep(0.5)
        assert seen('"sp":22.5}'), mlog.read_text()
        out = sox(port, 'get:/service/mqtt.activeClientId,/service/mqtt/sp.out')
        assert 'sp.out = 22.5' in out, out
        cid = re.search(r'activeClientId = (\S+)', out)
        return 'MQTT pub/sub via 10.0.2.2 (client %s)' % (cid.group(1) if cid else '?')
    finally:
        broker.terminate()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 21876
    if not (FW / 'esp32-sedona.bin').exists():
        sys.exit('first: ./spro build esp32 && ./spro fw esp32 --fw qemu --docker')
    JAVA.mkdir(parents=True, exist_ok=True)
    subprocess.run(['javac', '-nowarn', '-cp', str(HOME / 'lib/sedona.jar'), '-d', str(JAVA),
                    str(HERE / 'SoxProbe.java')], check=True)
    docker('rm', '-f', NAME)
    uid = '%d:%d' % (os.getuid(), os.getgid())
    r = docker('run', '-d', '--name', NAME, '--network', 'host', '-v', '%s:%s' % (ROOT, ROOT),
               '-u', uid, '-e', 'HOME=/tmp', IMAGE,
               str(ROOT / 'targets/esp32/tools/qemu_run.sh'), str(FW), str(port))
    if r.returncode:
        sys.exit(r.stderr)
    failed = 0
    try:
        end = time.time() + 180
        while time.time() < end and not re.search(r'App\] running|Cannot run|Guru', log()):
            time.sleep(2)
        text = log()
        checks = []
        assert 'unaligned loads broken' not in text, 'unaligned loads'
        assert '[sys::App] running' in text, text[-3000:]
        checks.append('boot, VM running')
        info = sox(port, 'info')
        assert 'sproPlatformEsp32::Esp32PlatformService' in info, info
        heap = re.search(r'memAvailable = (\d+)', info)
        checks.append('SOX info (heap %s)' % (heap.group(1) if heap else '?'))
        if 'sproHal' in (ROOT / 'build/esp32/kits.txt').read_text():
            out = sox(port, 'get:/io/board.board,/io/board.backend')
            assert '/io/board.backend = esp32' in out, out
            checks.append('sproHal board esp32')
        if 'sproMqtt' in (ROOT / 'build/esp32/kits.txt').read_text():
            checks.append(mqtt(port))
        sox(port, 'write')
        sox(port, 'save')
        checks.append('write + save')
        for c in checks:
            print('PASS', c)
    except AssertionError as e:
        failed = 1
        print('FAIL', str(e)[-3000:])
    finally:
        docker('rm', '-f', NAME)
    print('qemu: %d failed' % failed)
    return failed


if __name__ == '__main__':
    sys.exit(main())
