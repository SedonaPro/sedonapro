#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Functional checks of the svm runtime features of stage 2.

Scan-cycle detection with sproPlatformLinux::UnixPlatformService (2 ms
free-time sleeps), statistics, systemd notify/watchdog (emulated socket and
a real `systemd-run --user` unit), safe boot levels and rt.* settings.
Isolated ports: SOX 31876, web 38080, UI 38088.

usage: python3 tests/svm_stage2_test.py [--arch amd64] [-k name]
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
import svm_stage1_test as t1  # noqa: E402

SOX = 31876
t1.SOX_PORT = SOX


def app(out, name='stage2-app', **kw):
    kw.setdefault('sox', SOX)
    kw.setdefault('web', 38080)
    kw.setdefault('ui', 38088)
    return t1.prepare_app(out, name=name, **kw)


def fresh(out, name, sab):
    return t1.fresh_dir(out, name, sab, root='stage2-test')


def stats_lines(text):
    rows = []
    for line in text.splitlines():
        if line.startswith('svm stats: '):
            rows.append(dict(kv.split('=') for kv in line[len('svm stats: '):].split()))
    return rows


class NotifySocket:
    """Minimal stand-in for systemd's notification socket."""

    def __init__(self, path):
        self.path = str(path)
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self.sock.bind(self.path)
        self.sock.setblocking(False)
        self.messages = []

    def drain(self):
        while True:
            try:
                data = self.sock.recv(4096).decode()
            except BlockingIOError:
                return self.messages
            self.messages.append((time.time(), data))

    def count(self, text, since=0):
        return sum(1 for t, m in self.drain() if t >= since and text in m)

    def close(self):
        self.sock.close()
        os.unlink(self.path)


# ---------------------------------------------------------------- tests

def test_scan_detection_and_stats(out, sab):
    """2 ms platform sleeps must not count as scan cycles (scanPeriod 50 ms)."""
    sab = app(out, 'stage2-unixplat', platform='sproPlatformLinux::UnixPlatformService')
    d = fresh(out, 'stats', sab)
    vm = t1.Vm(d, 'stats.interval=2', 'stats.file=stats.txt')
    assert t1.started(vm)
    assert vm.wait_for('svm stats:', 10)
    time.sleep(4.5)
    log = vm.stop()
    rows = stats_lines(log)[1:]          # first window includes start-up
    assert rows, log[-2000:]
    for r in rows:
        cycles, avg = int(r['cycles']), float(r['period_ms_avg'])
        assert 30 <= cycles <= 45, 'cycles per 2 s: %s' % r
        assert 45.0 <= avg <= 55.0, 'average period: %s' % r
    text = (d / 'stats.txt').read_text()
    assert 'period_ms_avg=' in text and 'time=' in text


def test_notify_and_watchdog_socket(out, sab):
    d = fresh(out, 'notify', app(out))
    ns = NotifySocket(d / 'notify.sock')
    try:
        env = {'NOTIFY_SOCKET': ns.path, 'WATCHDOG_USEC': '1000000'}
        vm = t1.Vm(d, 'stats.interval=1', env=env)
        assert t1.started(vm)
        time.sleep(3)
        assert ns.count('READY=1') == 1
        t0 = time.time()
        time.sleep(3)
        pings = ns.count('WATCHDOG=1', t0)
        assert 5 <= pings <= 7, 'pings in 3 s at 0.5 s interval: %d' % pings
        assert ns.count('STATUS=scan avg') >= 2
        # a frozen VM sends nothing
        vm.proc.send_signal(signal.SIGSTOP)
        ns.drain()
        t0 = time.time()
        time.sleep(2)
        assert ns.count('WATCHDOG=1', t0) == 0
        vm.proc.send_signal(signal.SIGCONT)
        time.sleep(1.5)
        assert ns.count('WATCHDOG=1', t0) >= 2
        log = vm.stop()
        assert 'ping every 500 ms' in log
    finally:
        ns.close()


def test_watchdog_off(out, sab):
    d = fresh(out, 'watchdog-off', app(out))
    ns = NotifySocket(d / 'notify.sock')
    try:
        env = {'NOTIFY_SOCKET': ns.path, 'WATCHDOG_USEC': '1000000'}
        vm = t1.Vm(d, 'watchdog=0', env=env)
        assert t1.started(vm)
        time.sleep(2)
        assert ns.count('READY=1') == 1
        assert ns.count('WATCHDOG=1') == 0
        vm.stop()
    finally:
        ns.close()


def systemctl(*args):
    return subprocess.run(['systemctl', '--user', *args], capture_output=True, text=True)


def test_systemd_watchdog_real(out, sab):
    """Type=notify unit: READY gates start-up, a frozen VM is killed by systemd."""
    if shutil.which('systemd-run') is None or systemctl('is-system-running').returncode not in (0, 1):
        print('  skipped: no user systemd')
        return
    d = fresh(out, 'systemd', app(out))
    unit = 'svm-stage2-wd-%d' % os.getpid()
    t0 = time.time()
    r = subprocess.run(['systemd-run', '--user', '--unit', unit, '-p', 'Type=notify',
                        '-p', 'WatchdogSec=3', '-p', 'NotifyAccess=main',
                        '-p', 'WorkingDirectory=' + str(d),
                        str(d / 'svm'), '--plat'], capture_output=True, text=True, timeout=60)
    try:
        assert r.returncode == 0, r.stderr
        assert systemctl('show', '-P', 'ActiveState', unit).stdout.strip() == 'active'
        pid = int(systemctl('show', '-P', 'MainPID', unit).stdout.strip())
        time.sleep(5)                    # longer than WatchdogSec: pings keep it alive
        assert systemctl('show', '-P', 'ActiveState', unit).stdout.strip() == 'active'
        os.kill(pid, signal.SIGSTOP)
        end = time.time() + 20
        result = ''
        while time.time() < end:
            result = systemctl('show', '-P', 'Result', unit).stdout.strip()
            if result == 'watchdog':
                break
            time.sleep(0.5)
        assert result == 'watchdog', 'Result=%s' % result
        print('  frozen VM killed by systemd watchdog (Result=watchdog)')
    finally:
        systemctl('stop', unit)
        systemctl('reset-failed', unit)


def test_safeboot_levels(out, sab):
    base = app(out)
    bak = app(out, 'stage2-bak', app_name='bakapp')
    safe = app(out, 'stage2-safe', app_name='safeapp')
    d = fresh(out, 'safeboot', base)
    shutil.copy2(bak, d / 'app.sab.bak')
    shutil.copy2(safe, d / 'safe.sab')
    opts = ('safeboot.after=2', 'safeboot.app=safe.sab', 'safeboot.healthy.seconds=5')

    def crash_once():
        vm = t1.Vm(d, *opts)
        assert t1.started(vm), vm.text()[-2000:]
        vm.proc.send_signal(signal.SIGSEGV)
        vm.wait_exit(10)
        return vm.stop()

    crash_once()
    crash_once()
    log = crash_once()                               # start 3: level 1
    assert 'level 1, switching to the backup app' in log, log[-2000:]
    assert t1.sha(d / 'app.sab.safeboot1') == t1.sha(base)
    crash_once()                                     # start 4
    vm = t1.Vm(d, *opts)                             # start 5: level 2
    assert t1.started(vm)
    assert vm.wait_for('start is healthy', 15), vm.text()[-2000:]
    log = vm.stop()
    assert 'level 2, switching to safeboot.app' in log
    assert t1.sha(d / 'app.sab.safeboot2') == t1.sha(bak)
    assert (d / 'svm.boots').read_text().strip() == '0'
    # the running app was the safe one (App.save may have rewritten app.sab since)
    assert 'started with safe.sab' in log


def test_rt_settings(out, sab):
    d = fresh(out, 'rt', app(out))
    vm = t1.Vm(d, 'rt.mlockall=1', 'rt.cpus=0')
    assert t1.started(vm)
    status = Path('/proc/%d/status' % vm.proc.pid).read_text()
    log = vm.stop()
    assert 'memory locked' in log and 'CPU affinity 0' in log, log[:3000]
    assert re.search(r'Cpus_allowed_list:\s*0\n', status), status
    assert int(re.search(r'VmLck:\s*(\d+)', status).group(1)) > 0


def test_rt_strict(out, sab):
    """Unprivileged SCHED_FIFO fails: warning by default, refusal with rt.strict."""
    if os.geteuid() == 0:
        print('  skipped: running as root')
        return
    d = fresh(out, 'rt-strict', app(out))
    vm = t1.Vm(d, 'rt.sched=fifo:10')
    assert t1.started(vm)
    log = vm.stop()
    assert 'cannot set scheduling fifo:10' in log
    vm = t1.Vm(d, 'rt.sched=fifo:10', 'rt.strict=1')
    code = vm.wait_exit(10)
    log = vm.stop()
    assert code == 31 and 'refusing to start' in log, (code, log[-1000:])


TESTS = [test_scan_detection_and_stats, test_notify_and_watchdog_socket, test_watchdog_off,
         test_systemd_watchdog_real, test_safeboot_levels, test_rt_settings, test_rt_strict]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', default='amd64')
    parser.add_argument('-k', help='run tests whose name contains this text')
    args = parser.parse_args()
    out = t1.stage_out(args.arch)
    subprocess.run(['javac', '-nowarn', '-cp', str(t1.HOME / 'lib/sedona.jar'), '-d', str(t1.JAVA),
                    str(t1.HERE / 'SoxAction.java')], check=True)
    failed = 0
    for test in TESTS:
        if args.k and args.k not in test.__name__:
            continue
        t0 = time.time()
        try:
            test(out, None)
            print('PASS %-40s %5.1fs' % (test.__name__, time.time() - t0), flush=True)
        except Exception as e:  # noqa: BLE001
            failed += 1
            print('FAIL %-40s %s' % (test.__name__, str(e)[-1500:]), flush=True)
        finally:
            for vm in list(t1.Vm.alive):
                vm.stop()
    print('stage2: %d failed' % failed)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
