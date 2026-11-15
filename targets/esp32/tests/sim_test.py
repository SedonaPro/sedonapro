#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""ESP32 simulator checks (needs: ./spro build sim --add sproModbusMaster,sproModbusSlave,sproMqtt).

  python3 tests/sim_test.py [-k name]

unit tests in the VM, platform start, SOX (info, values, write, add+save,
reboot), task watchdog on a hung scan loop, safeboot after failed starts,
heap limit.  The app's ports are remapped (ESP32SIM_PORTS: SOX 1876 -> 21877,
Modbus 1502 -> 21502, web 8080 -> 28080, UI 8088 -> 28088), so a QEMU, svm or
other program on the usual ports does not interfere; the bench service (11876) is not touched.
"""
import argparse
import os
from pathlib import Path
import shutil
import signal
import socket
import struct
import urllib.request
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
HOME = ROOT / 'build' / 'home'
SIM = ROOT / 'build' / 'sim'
JAVA_OUT = SIM / 'java'
OUT = SIM / 'test'
PASSWORD = os.environ.get('SEDONA_ADMIN_PASSWORD', 'sedonapro-dev')


def flash(name):
    d = OUT / name
    if d.exists():
        shutil.rmtree(d)
    shutil.copytree(SIM / 'data', d)
    return d


class Chip:
    def __init__(self, name, *opts):
        self.dir = flash(name)
        self.log = OUT / (name + '.log')
        self.fh = self.log.open('w')
        self.proc = subprocess.Popen([str(SIM / 'esp32sim'), '--flash', str(self.dir),
                                      '--wdt', '3', '--restart-delay', '200', *opts],
                                     stdout=self.fh, stderr=subprocess.STDOUT,
                                     env=dict(os.environ, SPRO_GPIO_PORT=str(GPIO_PORT), ESP32SIM_PORTS=PORTS))

    def text(self):
        return self.log.read_text(errors='replace')

    def count(self, needle):
        return self.text().count(needle)

    def wait(self, cond, timeout=30):
        end = time.time() + timeout
        while time.time() < end:
            if cond():
                return True
            if self.proc.poll() is not None:
                return cond()
            time.sleep(0.1)
        return False

    def wait_boot(self, n, timeout=30):
        """n-th boot (0 = power on) has its scan loop running."""
        return self.wait(lambda: self.count('watchdog armed') > n, timeout)

    def hang(self):
        self.proc.send_signal(signal.SIGUSR2)

    def stop(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
        self.fh.close()


def sox(cmd, timeout=60, home=None, props=()):
    cp = os.pathsep.join([str(HOME / 'lib/sedona.jar'), str(JAVA_OUT)])
    r = subprocess.run(['java', '-Dsedona.home=' + str(home or HOME), *props, '-cp', cp,
                        'SoxProbe', '127.0.0.1', str(SOX_PORT), cmd],
                       env=dict(os.environ, SEDONA_ADMIN_PASSWORD=PASSWORD),
                       capture_output=True, text=True, timeout=timeout)
    if 'PROBE %s OK' % cmd not in r.stdout:
        raise AssertionError('SOX %s failed: %s %s' % (cmd, r.stdout, r.stderr[-800:]))
    return r.stdout


def test_unit():
    d = flash('unit')
    r = subprocess.run([str(SIM / 'esp32sim'), '--flash', str(d), '--tests'],
                       capture_output=True, text=True, timeout=300)
    assert 'Assert Failures  = 0' in r.stdout and r.returncode == 0, r.stdout[-1500:]
    return r.stdout.split('Assert Successes = ')[1].split()[0] + ' asserts'


def test_platform_sox():
    chip = Chip('sox')
    try:
        assert chip.wait_boot(0), chip.text()[-2000:]
        info = sox('info')
        assert 'sproPlatformEsp32::Esp32PlatformService' in info and 'resetReason = 1' in info, info
        sox('values')
        sox('write')
        added = sox('add').split('added /play/')[1].split()[0]
        sox('save')
        assert 'saved' in chip.text()
        sox('reboot')
        assert chip.wait_boot(1), chip.text()[-2000:]
        assert 'rst:3' in chip.text()
        assert added in sox('list'), 'component lost after reboot'
        assert 'resetReason = 3' in sox('info')
        return 'added %s survived a reboot' % added
    finally:
        chip.stop()


def test_manifests():
    """A SOX client with an empty manifest cache (fresh MCS) gets the schema
    from the device: m:m.zip and m:<kit>-<checksum>.xml for every kit."""
    if not (SIM / 'data/manifests').exists():
        return 'skipped: built with --manifests none'
    chip = Chip('manifests')
    home = OUT / 'empty-home'
    if home.exists():
        shutil.rmtree(home)
    home.mkdir()
    try:
        assert chip.wait_boot(0), chip.text()[-2000:]
        out = sox('manifests', home=home, props=['-Dprobe.out=' + str(OUT / 'm-from-device.zip')])
        assert 'refused' in out and 'was served' not in out, out
        cached = sorted(f.name for f in (home / 'manifests').rglob('*.xml'))
        note = ''
        ui = SIM / 'data/manifests/ui.zip'
        out = sox('files', props=['-Dprobe.ui=' + str(ui)] if ui.exists() else [])
        assert 'refused' in out and 'was served' not in out, out
        if ui.exists():
            note = ', m:ui.zip %d bytes' % ui.stat().st_size
        return '%d kits downloaded into an empty cache, m.zip %d bytes%s' % (
            len(cached), (OUT / 'm-from-device.zip').stat().st_size, note)
    finally:
        chip.stop()


def test_watchdog():
    chip = Chip('wdt')
    try:
        assert chip.wait_boot(0)
        t0 = time.time()
        chip.hang()
        assert chip.wait(lambda: 'rst:6' in chip.text(), 15), chip.text()[-2000:]
        assert 'Task watchdog got triggered' in chip.text()
        assert chip.wait_boot(1)
        return 'reset after %.1f s (wdt 3 s)' % (time.time() - t0)
    finally:
        chip.stop()


def test_safeboot():
    chip = Chip('safeboot')
    try:
        assert chip.wait_boot(0)
        sox('write')                       # change the app and save it: app.sab.bak
        sox('save')                        # now holds a different, valid app
        for n in range(3):                 # safeboot.after=3: three failed starts ...
            assert chip.wait_boot(n), chip.text()[-2000:]
            chip.hang()
            assert chip.wait(lambda: chip.count('ESP-ROM') > n + 1, 15), chip.text()[-2000:]
        assert chip.wait_boot(3)           # ... and the 4th runs the backup app
        assert 'started with app.sab.bak' in chip.text(), chip.text()[-3000:]
        assert (chip.dir / 'app.sab.safeboot1').exists()
        return 'backup app after 3 failed starts'
    finally:
        chip.stop()


def test_heap_limit():
    d = flash('heap')
    size = (d / 'kits.scode').stat().st_size
    r = subprocess.run([str(SIM / 'esp32sim'), '--flash', str(d), '--heap', str(size // 2),
                        '--no-restart'], capture_output=True, text=True, timeout=30)
    assert r.returncode == 103, (r.returncode, r.stdout[-1500:])   # VM exit -> SW reset
    assert 'Cannot' in r.stdout or 'malloc' in r.stdout.lower(), r.stdout[-1500:]
    return 'VM refuses to start with %d bytes of heap' % (size // 2)


KIT_MODULES = {'web': 'web', 'sproExpr': 'expression', 'sproModbusMaster': 'modbus', 'sproMqtt': 'mqtt',
               'sproHal': 'hal'}
GPIO_PORT = 18090
SOX_PORT = int(os.environ.get('SIM_SOX_PORT', '21877'))
MODBUS_PORT, WEB_PORT = 21502, 28080
PORTS = '1876=%d,1502=%d,8080=%d,8088=28088' % (SOX_PORT, MODBUS_PORT, WEB_PORT)


def modules():
    f = SIM / 'kits.txt'
    kits = f.read_text().split() if f.exists() else []
    return [m for k, m in KIT_MODULES.items() if k in kits]


def modbus_read_float(port=None, addr=0):
    port = port or MODBUS_PORT
    with socket.create_connection(('127.0.0.1', port), timeout=3) as s:
        s.sendall(struct.pack('>HHHBBHH', 1, 0, 6, 1, 3, addr, 2))
        r = s.recv(256)
    assert not r[7] & 0x80, 'modbus exception %d' % r[8]
    return struct.unpack('>f', r[9:13])[0]


def test_modules():
    mods = modules()
    chip = Chip('modules')
    broker = None
    notes = []
    try:
        assert chip.wait_boot(0), chip.text()[-2000:]
        time.sleep(2)
        if 'web' in mods:
            body = urllib.request.urlopen('http://127.0.0.1:%d/' % WEB_PORT, timeout=5).read().decode()
            assert 'sproPlatformEsp32::Esp32PlatformService' in body, body[:300]
            # request in pieces with pauses: must be answered, not rejected
            with socket.create_connection(('127.0.0.1', WEB_PORT), timeout=10) as c:
                for part in (b'GET / HT', b'TP/1.0\r\nHost: x\r', b'\n\r\n'):
                    c.sendall(part)
                    time.sleep(0.3)
                head = c.recv(64)
            assert head.startswith(b'HTTP/1.0 200') or head.startswith(b'HTTP/1.1 200'), head
            # a client that sends nothing gets 408 after the request timeout
            with socket.create_connection(('127.0.0.1', WEB_PORT), timeout=10) as c:
                head = c.recv(64)
            assert b' 408' in head.split(b'\r\n')[0], head
            notes.append('web')
        if 'expression' in mods:
            out = sox('get:/play/formula.ok,/play/formula.out')
            assert 'formula.ok = true' in out, out
            notes.append('expression')
        if 'modbus' in mods:
            out = sox('get:/mbMastr/dev1/reg0.status,/mbMastr/dev1/reg0.out')
            assert 'reg0.status = 2' in out, out            # statusOk
            v = modbus_read_float()
            assert 0.0 <= v <= 100.0, v                     # rampA range
            notes.append('modbus %.1f' % v)
        if 'hal' in mods:
            import json

            def gpio(path):
                with urllib.request.urlopen('http://127.0.0.1:%d%s' % (GPIO_PORT, path), timeout=5) as r:
                    return json.loads(r.read().decode())

            def value(state, name):
                return next(c['value'] for c in state['channels'] if c['name'] == name)
            assert chip.wait(lambda: 'GPIO panel' in chip.text(), 10), chip.text()[-1500:]
            assert value(gpio('/api/state'), 'DO1') == 0
            gpio('/api/set?ch=DI1&v=1')                       # press the button ...
            assert chip.wait(lambda: value(gpio('/api/state'), 'DO1') == 1, 5), 'DO1 did not follow DI1'
            gpio('/api/set?ch=DI1&v=0')
            assert chip.wait(lambda: value(gpio('/api/state'), 'DO1') == 0, 5)
            ao = value(gpio('/api/state'), 'AO1')
            assert 0 <= ao <= 100, ao
            out = sox('get:/io/board.board,/io/lamp.status')
            assert 'board.board = sim' in out, out
            notes.append('hal DI1->DO1, AO1 %.0f%%' % ao)
        if 'mqtt' in mods:
            mlog = OUT / 'mqtt.log'
            log = mlog.open('w')
            broker = subprocess.Popen([sys.executable, str(ROOT / 'tools/mqtt_broker.py'), '18830',
                                       '--user', 'sim', '--password', 'pw'],
                                      stdout=log, stderr=subprocess.STDOUT)
            time.sleep(0.5)
            seen = lambda s: s in mlog.read_text()
            # wrong password first: refused, lastError auth
            for c in ('port=18830', 'username=sim', 'password=bad', 'enabled=true'):
                sox('set:/service/mqtt.' + c)
            assert chip.wait(lambda: 'lastError = 6' in sox('get:/service/mqtt.lastError'), 10), mlog.read_text()
            sox('set:/service/mqtt.password=pw')
            assert chip.wait(lambda: seen('PUBLISHr sedonapro/esp32/status online'), 15), mlog.read_text()
            assert chip.wait(lambda: seen('PUBLISHr sedonapro/esp32/sum '), 5), mlog.read_text()
            assert chip.wait(lambda: seen('PUBLISH sedonapro/esp32/state {"sum":'), 5), mlog.read_text()
            pub = lambda t, v: subprocess.run([sys.executable, str(ROOT / 'tools/mqtt_broker.py'), 'pub',
                                               '127.0.0.1', '18830', 'sedonapro/esp32/' + t, v,
                                               '--user', 'sim', '--password', 'pw'], check=True)
            assert chip.wait(lambda: seen('SUBSCRIBE sedonapro/esp32/cmd/mode'), 5), mlog.read_text()
            pub('cmd/sp', '22.5')
            pub('cmd/json', '{"room": {"t": 19.25, "rh": 40}}')
            pub('cmd/mode', 'ON')
            want = ('sp.out = 22.5', 'room.out = 19.25', 'mode.out = true', 'sp.status = 2')
            get = 'get:/service/mqtt/sp.out,/service/mqtt/sp.status,/service/mqtt/room.out,/service/mqtt/mode.out'
            assert chip.wait(lambda: all(w in sox(get) for w in want), 10), sox(get)
            assert chip.wait(lambda: seen('"sp":22.5}'), 5), mlog.read_text()
            pub('cmd/sp', 'abc')
            assert chip.wait(lambda: 'sp.status = 6' in sox(get), 5), sox(get)      # badValue
            out = sox('get:/service/mqtt.state,/service/mqtt.sent,/service/mqtt.activeClientId')
            assert 'mqtt.state = 3' in out, out
            chip.proc.kill()                                # power cut: broker sends the will
            assert chip.wait(lambda: seen('WILL sedonapro/esp32/status offline'), 10), mlog.read_text()
            notes.append('mqtt auth, LWT, pub/sub, JSON')
        return ', '.join(notes) or 'no modules'
    finally:
        if broker:
            broker.terminate()
        chip.stop()


TESTS = [test_unit, test_modules, test_platform_sox, test_manifests, test_watchdog, test_safeboot, test_heap_limit]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('-k')
    args = p.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    JAVA_OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run(['javac', '-nowarn', '-cp', str(HOME / 'lib/sedona.jar'), '-d', str(JAVA_OUT),
                    str(HERE / 'SoxProbe.java')], check=True)
    failed = 0
    for t in TESTS:
        if args.k and args.k not in t.__name__:
            continue
        t0 = time.time()
        try:
            note = t()
            print('PASS %-22s %5.1fs  %s' % (t.__name__, time.time() - t0, note or ''), flush=True)
        except Exception as e:  # noqa: BLE001
            failed += 1
            print('FAIL %-22s %s' % (t.__name__, str(e)[-1500:]), flush=True)
    print('sim: %d failed' % failed)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
