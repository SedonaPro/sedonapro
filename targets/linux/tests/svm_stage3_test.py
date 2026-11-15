#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Functional checks of stage 3: scan.resync and modbus.safe.

scan.resync: the VM is frozen (SIGSTOP) for 2 s; afterwards the legacy loop
catches up the missed cycles in a burst, skip/now do not.
modbus.safe: loopback Modbus TCP slave + master inside the test VM.
  - readdr: the device address is changed while a request is in flight;
    legacy code reverts the change, modbus.safe keeps it;
  - churn: points are added and removed under AddressSanitizer
    (svm.asan from `./spro build linux --asan`), which must
    report no memory errors and, with modbus.safe, no leaked registers.
Isolated ports: SOX 41876, web 48080, UI 48088, Modbus 15120.

usage: python3 tests/svm_stage3_test.py [--arch amd64] [-k name]
"""
import argparse
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import svm_stage1_test as t1  # noqa: E402

SOX = 41876
MB_PORT = 15120
t1.SOX_PORT = SOX


def app(out):
    return t1.prepare_app(out, name='stage3-app', sox=SOX, web=48080, ui=48088)


def fresh(out, name):
    return t1.fresh_dir(out, name, app(out), root='stage3-test')


def stats_rows(text):
    rows = []
    for line in text.splitlines():
        if line.startswith('svm stats: '):
            rows.append(dict(kv.split('=') for kv in line[len('svm stats: '):].split()))
    return rows


def run_freeze(out, mode):
    """Run 7 s with a 2 s freeze; return (cycles, seconds, rows, log)."""
    d = fresh(out, 'resync-' + mode)
    vm = t1.Vm(d, 'stats.interval=1', 'scan.resync=' + mode)
    assert t1.started(vm)
    assert vm.wait_for('svm stats:', 5)
    time.sleep(1.5)
    vm.proc.send_signal(signal.SIGSTOP)
    time.sleep(2)
    vm.proc.send_signal(signal.SIGCONT)
    time.sleep(4)
    log = vm.stop()
    rows = stats_rows(log)[1:]                      # skip the start-up window
    cycles = sum(int(r['cycles']) for r in rows)
    seconds = sum(float(r['window_s']) for r in rows)
    return cycles, seconds, rows, log


def test_resync_off_catches_up(out):
    cycles, seconds, rows, _ = run_freeze(out, 'off')
    # legacy: the burst restores the nominal 20 cycles/s over the run
    assert cycles >= 0.9 * seconds * 20, (cycles, seconds)
    assert min(float(r['period_ms_min']) for r in rows) < 5.0, 'no catch-up burst seen'
    assert all(r['resyncs'] == '0' for r in rows)
    print('  off: %d cycles in %.1f s' % (cycles, seconds))


def test_resync_skip(out):
    cycles, seconds, rows, log = run_freeze(out, 'skip')
    lost = 2.0 * 20
    assert cycles <= seconds * 20 - 0.7 * lost, (cycles, seconds)
    assert 'svm scan: overrun' in log and '(skip)' in log
    last = rows[-1]
    assert int(last['resyncs']) >= 1 and int(last['skipped_periods']) >= 30, last
    assert int(last['total_overruns']) >= 1
    print('  skip: %d cycles in %.1f s, skipped %s periods' % (cycles, seconds, last['skipped_periods']))


def test_resync_now(out):
    cycles, seconds, rows, log = run_freeze(out, 'now')
    assert cycles <= seconds * 20 - 0.7 * 40, (cycles, seconds)
    assert '(now)' in log
    print('  now: %d cycles in %.1f s' % (cycles, seconds))


def stress(mode, n, timeout=300):
    env = dict(os.environ, SEDONA_ADMIN_PASSWORD=t1.PASSWORD)
    cp = os.pathsep.join([str(t1.HOME / 'lib/sedona.jar'), str(t1.JAVA)])
    r = subprocess.run(['java', '-Dsedona.home=' + str(t1.HOME), '-cp', cp, 'ModbusStress',
                        str(SOX), str(MB_PORT), mode, str(n)],
                       env=env, capture_output=True, text=True, timeout=timeout)
    return r.stdout + r.stderr


def readdr(out, safe, n=8):
    d = fresh(out, 'readdr-%d' % safe)
    vm = t1.Vm(d, 'modbus.safe=%d' % safe)
    assert t1.started(vm)
    text = stress('readdr', n)
    vm.stop()
    m = re.search(r'READDR recovered (\d+)/(\d+)', text)
    assert m, text[-2000:]
    return int(m.group(1)), int(m.group(2))


def test_modbus_readdr_values(out):
    """Device address change on a live link: values keep flowing (both modes)."""
    for safe in (0, 1):
        ok, n = readdr(out, safe)
        assert ok == n, 'modbus.safe=%d recovered only %d/%d' % (safe, ok, n)
    print('  value after device address change: %d/%d in both modes' % (ok, n))


def blackhole(out, safe, n=10):
    """Return the register addresses seen by the backend after each change."""
    import socket
    import threading
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(('127.0.0.1', MB_PORT + 1))
    srv.listen(4)
    stop = threading.Event()
    conns = []

    def serve():
        srv.settimeout(0.2)
        while not stop.is_set():
            try:
                conn, _ = srv.accept()
                conns.append(conn)               # read nothing, answer nothing
            except OSError:
                pass

    th = threading.Thread(target=serve, daemon=True)
    th.start()
    d = fresh(out, 'blackhole-%d' % safe)
    try:
        vm = t1.Vm(d, 'modbus.safe=%d' % safe)
        assert t1.started(vm)
        text = stress('blackhole', n)
        assert 'BLACKHOLE DONE' in text, text[-2000:]
        log = vm.stop()
    finally:
        stop.set()
        th.join()
        for conn in conns:
            conn.close()
        srv.close()
    seen = [int(m) for m in re.findall(r'\nsedona\ndev_addr: *\d+\nfunc: *\d+\naddr: *(\d+)', log)]
    assert len(seen) == n, log[-3000:]
    return seen


def test_modbus_config_race_legacy(out):
    seen = blackhole(out, 0)
    lost = sum(1 for i, a in enumerate(seen) if a != 10 + i)
    print('  legacy: %d of %d address changes reverted by an in-flight request %s' % (lost, len(seen), seen))


def test_modbus_config_race_safe(out):
    seen = blackhole(out, 1)
    assert seen == [10 + i for i in range(len(seen))], seen
    print('  modbus.safe: all %d address changes kept' % len(seen))


def churn_asan(out, safe, n=30, extra=()):
    asan = out / 'svm.asan'
    if not asan.exists():
        raise AssertionError('svm.asan missing: ./spro build linux --asan')
    d = fresh(out, 'churn-asan-%d%s' % (safe, '-mem' if extra else ''))
    shutil.copy2(asan, d / 'svm')
    env = {'ASAN_OPTIONS': 'verify_asan_link_order=0:detect_leaks=1:halt_on_error=1',
           'LSAN_OPTIONS': 'report_objects=0'}
    vm = t1.Vm(d, 'modbus.safe=%d' % safe, *extra, env=env)
    assert t1.started(vm, 1), vm.text()[-3000:]
    text = stress('churn', n)
    assert 'CHURN OK' in text, text[-2000:]
    time.sleep(3)                     # network removed: backend thread exits
    t1.sox_action('quit', SOX)        # orderly exit so LeakSanitizer runs
    code = vm.wait_exit(60)
    log = vm.stop()
    assert 'ERROR: AddressSanitizer' not in log, log[log.find('ERROR: AddressSanitizer'):][:4000]
    leaks = [blk for blk in log.split('\n\n') if 'Direct leak' in blk or 'Indirect leak' in blk]
    reg_leaks = [blk for blk in leaks if 'ModbusPoint_addRegister' in blk]
    return code, reg_leaks, leaks


def test_modbus_churn_asan_legacy(out):
    code, reg_leaks, leaks = churn_asan(out, 0)
    total = sum(int(m) for blk in reg_leaks for m in re.findall(r'leak of \d+ byte\(s\) in (\d+) object', blk))
    print('  legacy: exit %s, %d leaked register(s) of removed points, %d leak report(s) in total'
          % (code, total, len(leaks)))
    assert total >= 30


def test_modbus_churn_asan_safe(out):
    code, reg_leaks, leaks = churn_asan(out, 1)
    print('  modbus.safe: exit %s, no memory errors, %d leak report(s) elsewhere' % (code, len(leaks)))
    assert not reg_leaks, reg_leaks[0][:3000]


def leak_sites(leaks):
    sites = {'slave mapping': 'modbus_mapping_new', 'expression lexer': '_lex_init'}
    return {name for blk in leaks for name, key in sites.items() if key in blk}


def test_mem_fixes_asan(out):
    _, _, before = churn_asan(out, 1)
    _, reg_leaks, after = churn_asan(out, 1, extra=('mem.fixes=1',))
    assert not reg_leaks
    print('  leaks without mem.fixes: %s; with mem.fixes: %s; remaining reports: %d'
          % (sorted(leak_sites(before)), sorted(leak_sites(after)) or 'none', len(after)))
    assert leak_sites(before) == {'slave mapping', 'expression lexer'}, leak_sites(before)
    assert not leak_sites(after)


TESTS = [test_resync_off_catches_up, test_resync_skip, test_resync_now,
         test_modbus_readdr_values, test_modbus_config_race_legacy, test_modbus_config_race_safe,
         test_modbus_churn_asan_legacy, test_modbus_churn_asan_safe, test_mem_fixes_asan]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', default='amd64')
    parser.add_argument('-k', help='run tests whose name contains this text')
    args = parser.parse_args()
    out = t1.stage_out(args.arch)
    subprocess.run(['javac', '-nowarn', '-cp', str(t1.HOME / 'lib/sedona.jar'), '-d', str(t1.JAVA),
                    str(t1.HERE / 'SoxAction.java'), str(t1.HERE / 'ModbusStress.java')],
                   check=True)
    failed = 0
    for test in TESTS:
        if args.k and args.k not in test.__name__:
            continue
        t0 = time.time()
        try:
            test(out)
            print('PASS %-40s %5.1fs' % (test.__name__, time.time() - t0), flush=True)
        except Exception as e:  # noqa: BLE001
            failed += 1
            print('FAIL %-40s %s' % (test.__name__, str(e)[-2500:]), flush=True)
        finally:
            for vm in list(t1.Vm.alive):
                vm.stop()
    print('stage3: %d failed' % failed)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
