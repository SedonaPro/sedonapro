#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Functional checks of the optional svm runtime features (stage 1).

Runs isolated VMs in build/<arch>/stage1-test with their own ports
(SOX 21876, web 28080, UI 28088) and a throwaway admin password.
Covers: default clock, stage.validate, stage.rollback (VM error and crash
loop), trial acceptance, install during ERR_RESTART, save.atomic and
crash.handler.  Requires ./spro build linux.

usage: python3 targets/linux/tests/svm_stage1_test.py [--arch amd64] [-k name]
"""
import argparse
import base64
import hashlib
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from spro.home import sedonac as compiler, HOME, BUILD  # noqa: E402
from spro.project import PROJECT  # noqa: E402
JAVA = BUILD / 'tests-java'


def stage_out(arch, profile=None, board='sim'):
    """Work dir with the built svm and kits.scode (./spro build linux)."""
    profile = profile or PROJECT['targets']['linux']['default_profile']
    run = BUILD / ('linux-%s-%s-%s' % (arch, profile, board)) / 'run'
    if not (run / 'svm').exists():
        sys.exit('build first: ./spro build linux --arch %s' % arch)
    out = run.parent / 'stage-tests'
    out.mkdir(exist_ok=True)
    for f in ('svm', 'kits.scode', 'svm.asan'):
        if (run / f).exists():
            shutil.copy2(run / f, out / f)
    JAVA.mkdir(parents=True, exist_ok=True)
    return out

PASSWORD = 'stage1-test-only'
SOX_PORT = 21876
results = []


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Vm:
    """One svm --plat process; output goes to <dir>/svm-<n>.log."""
    count = 0
    alive = []

    def __init__(self, workdir, *opts, env=None):
        Vm.count += 1
        Vm.alive.append(self)
        self.log = workdir / ('svm-%d.log' % Vm.count)
        self.fh = self.log.open('w')
        # line-buffered stdout, as in sedonapro-sedona.service
        self.proc = subprocess.Popen(['stdbuf', '-oL', '-eL', str(workdir / 'svm'), '--plat', *['--opt=' + o for o in opts]],
                                     cwd=workdir, stdout=self.fh, stderr=subprocess.STDOUT,
                                     env=dict(os.environ, **(env or {})))

    def text(self):
        return self.log.read_text(errors='replace')

    def wait_for(self, needle, timeout=30):
        end = time.time() + timeout
        while time.time() < end:
            if needle in self.text():
                return True
            if self.proc.poll() is not None:
                return needle in self.text()
            time.sleep(0.2)
        return False

    def wait_exit(self, timeout=30):
        try:
            return self.proc.wait(timeout)
        except subprocess.TimeoutExpired:
            return None

    def stop(self):
        if self in Vm.alive:
            Vm.alive.remove(self)
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGCONT)
            self.proc.send_signal(signal.SIGTERM)
            if self.wait_exit(10) is None:
                self.proc.kill()
                self.proc.wait()
        self.fh.close()
        return self.text()  # file is reread from disk


def sox_action(action, port=None):
    env = dict(os.environ, SEDONA_ADMIN_PASSWORD=PASSWORD)
    cp = os.pathsep.join([str(HOME / 'lib/sedona.jar'), str(JAVA)])
    r = subprocess.run(['java', '-Dsedona.home=' + str(HOME), '-cp', cp, 'SoxAction',
                        '127.0.0.1', str(port or SOX_PORT), action],
                       env=env, capture_output=True, text=True, timeout=60)
    if 'ACTION %s OK' % action not in r.stdout:
        raise AssertionError('SOX %s failed: %s %s' % (action, r.stdout, r.stderr))


def prepare_app(out, name='stage1-app', sox=SOX_PORT, web=28080, ui=28088,
                platform='sys::PlatformService', app_name=None):
    """Compile an isolated app (test ports, no network config) once."""
    sab = out / (name + '.sab')
    if sab.exists():
        return sab
    app = ET.parse(ROOT / 'targets/linux/apps/defaultApp_empty.sax')
    if app_name:
        app.getroot().find("app/prop[@name='appName']").set('val', app_name)
    services = app.getroot().find("app/comp[@name='service']")
    services.find("comp[@name='plat']").set('type', platform)
    services.remove(services.find("comp[@name='network']"))
    ET.SubElement(services.find("comp[@name='sox']"), 'prop', name='port', val=str(sox))
    services.find("comp[@name='web']/prop[@name='port']").set('val', str(web))
    services.find("comp[@name='ui']/prop[@name='port']").set('val', str(ui))
    digest = base64.b64encode(hashlib.sha1(('admin:' + PASSWORD).encode()).digest()).decode()
    services.find("comp[@name='users']/comp[@name='admin']/prop[@name='cred']").set('val', digest)
    source = out / (name + '.sax')
    app.write(source, encoding='unicode')
    compiler(source)
    return sab


def incompatible_scode(out):
    """A valid image for this VM without the kits the test app needs."""
    image = out / 'stage1-minimal.scode'
    if not image.exists():
        source = out / 'stage1-minimal.xml'
        source.write_text('<sedonaCode endian="little" blockSize="4" refSize="8" '
                          'main="sys::Sys.main" debug="true" test="false">\n'
                          '  <depend on="sys 1.2" />\n  <depend on="inet 1.2" />\n'
                          '  <depend on="sox 1.2" />\n</sedonaCode>\n')
        compiler(source)
    return image


def fresh_dir(out, name, sab, root='stage1-test'):
    d = out / root / name
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    shutil.copy2(out / 'svm', d / 'svm')
    shutil.copy2(out / 'kits.scode', d / 'kits.scode')
    shutil.copy2(sab, d / 'app.sab')
    return d


def started(vm, count=1):
    """Wait until SoxService has started count times (once per VM run)."""
    end = time.time() + 40
    while time.time() < end:
        if vm.text().count('[sox::SoxService] started port') >= count:
            return True
        if vm.proc.poll() is not None:
            break
        time.sleep(0.2)
    return False


# ---------------------------------------------------------------- tests

def test_defaults(out, sab):
    d = fresh_dir(out, 'defaults', sab)
    vm = Vm(d)
    assert started(vm), 'VM did not start'
    sox_action('save')
    log = vm.stop()
    assert 'clock=monotonic' in log, 'monotonic clock is not the default'
    assert 'save.atomic=0' in log and 'stage.rollback=0' in log
    assert (d / 'app.sab.bak').exists(), 'legacy save did not create app.sab.bak'


def test_legacy_stage_unvalidated(out, sab):
    """Without stage.validate a staged file is installed as before."""
    d = fresh_dir(out, 'legacy-stage', sab)
    (d / 'app.sab.stage').write_bytes((d / 'app.sab').read_bytes())
    vm = Vm(d)
    assert started(vm)
    log = vm.stop()
    assert 'Moving app.sab.stage to app.sab' in log
    assert not (d / 'app.sab.stage').exists()


def test_validate_rejects(out, sab):
    d = fresh_dir(out, 'validate', sab)
    before = sha(d / 'kits.scode')
    (d / 'kits.scode.stage').write_bytes(b'not an scode image at all')
    (d / 'app.sab.stage').write_bytes((d / 'app.sab').read_bytes())
    vm = Vm(d, 'stage.validate=1')
    assert started(vm)
    log = vm.stop()
    assert 'rejected kits.scode.stage: bad magic' in log, log[-2000:]
    assert (d / 'kits.scode.rejected').exists() and (d / 'app.sab.rejected').exists()
    assert sha(d / 'kits.scode') == before


def test_validate_truncated_sab(out, sab):
    d = fresh_dir(out, 'validate-sab', sab)
    data = (d / 'app.sab').read_bytes()
    (d / 'app.sab.stage').write_bytes(data[:-10])
    vm = Vm(d, 'stage.validate=1')
    assert started(vm)
    log = vm.stop()
    assert 'missing end marker' in log, log[-2000:]
    assert sha(d / 'app.sab') == sha(sab)


def test_rollback_on_vm_error(out, sab):
    """Incompatible kits fail App.load; the VM restores the previous kits."""
    d = fresh_dir(out, 'rollback-error', sab)
    good = sha(d / 'kits.scode')
    shutil.copy2(incompatible_scode(out), d / 'kits.scode.stage')
    vm = Vm(d, 'stage.validate=1', 'stage.rollback=1')
    assert vm.wait_for('restored kits.scode from kits.scode.lkg', 60), vm.text()[-3000:]
    assert started(vm)
    time.sleep(2)
    assert vm.proc.poll() is None, 'VM did not keep running after rollback'
    sox_action('save')
    vm.stop()
    assert sha(d / 'kits.scode') == good
    assert (d / 'kits.scode.failed').exists()
    assert not (d / 'svm.trial').exists()


def test_rollback_after_crash_loop(out, sab):
    """A VM that dies during the trial is rolled back on the next start."""
    d = fresh_dir(out, 'rollback-crash', sab)
    good = sha(d / 'kits.scode')
    # a distinguishable but working copy of the same image
    shutil.copy2(d / 'kits.scode', d / 'kits.scode.stage')
    opts = ('stage.validate=1', 'stage.rollback=1', 'stage.trial.seconds=60',
            'stage.trial.attempts=2', 'crash.handler=1')
    for attempt in (1, 2):
        vm = Vm(d, *opts)
        assert started(vm)
        assert (d / 'svm.trial').exists()
        vm.proc.send_signal(signal.SIGSEGV)
        code = vm.wait_exit(10)
        log = vm.stop()
        assert code == -signal.SIGSEGV, 'exit %r' % code
        assert 'SVM FATAL SIGNAL' in log and 'backtrace' in log, log[-2000:]
    vm = Vm(d, *opts)
    assert vm.wait_for('2 starts without reaching 60 s', 30), vm.text()[-3000:]
    assert started(vm)
    vm.stop()
    assert not (d / 'svm.trial').exists()
    assert sha(d / 'kits.scode') == good


def test_trial_accept_and_restart_install(out, sab):
    """Update installed by App.restart (ERR_RESTART) and accepted after the trial."""
    d = fresh_dir(out, 'trial-accept', sab)
    vm = Vm(d, 'stage.validate=1', 'stage.rollback=1', 'stage.trial.seconds=5')
    assert started(vm)
    shutil.copy2(d / 'app.sab', d / 'app.sab.stage')
    sox_action('restart')
    assert vm.wait_for('Restarting VM', 20)
    assert vm.wait_for('update trial: accepted after 5 s', 20), vm.text()[-3000:]
    assert started(vm, 2)
    assert vm.wait_for('update trial: update accepted', 30), vm.text()[-3000:]
    vm.stop()
    assert not (d / 'svm.trial').exists()
    assert (d / 'app.sab.lkg').exists()


def test_atomic_save(out, sab):
    d = fresh_dir(out, 'atomic', sab)
    vm = Vm(d, 'save.atomic=1')
    assert started(vm)
    sox_action('save')
    good_bak = sha(d / 'app.sab')          # first save: app.sab -> bak, new app.sab
    assert (d / 'app.sab.bak').exists()
    bak_before = sha(d / 'app.sab.bak')
    # corrupt the current file: the next backup must not overwrite the good .bak
    data = (d / 'app.sab').read_bytes()
    (d / 'app.sab').write_bytes(data[:len(data) // 2])
    sox_action('save')
    log = vm.stop()
    assert 'keeping app.sab.bak, app.sab is invalid' in log, log[-2000:]
    assert sha(d / 'app.sab.bak') == bak_before
    assert sha(d / 'app.sab') == good_bak, 'saved app differs from the previous save'
    assert not list(d.glob('*.tmp'))


def test_save_keep(out, sab):
    d = fresh_dir(out, 'keep', sab)
    vm = Vm(d, 'save.atomic=1', 'save.keep=3')
    assert started(vm)
    inos = [(d / 'app.sab').stat().st_ino]
    for _ in range(4):
        sox_action('save')
        inos.append((d / 'app.sab').stat().st_ino)
    log = vm.stop()
    assert 'save.keep=3' in log, log[-2000:]
    # newest backup first: bak = previous app, .1 = one before, .2 = two before
    names = ['app.sab.bak', 'app.sab.bak.1', 'app.sab.bak.2']
    for i, n in enumerate(names):
        assert (d / n).exists(), n
    assert not (d / 'app.sab.bak.3').exists()
    assert [(d / n).stat().st_ino for n in names] == [inos[-2 - i] for i in range(3)], inos
    assert 'save.keep: cannot' not in log


def test_save_keep_default(out, sab):
    d = fresh_dir(out, 'keep1', sab)
    vm = Vm(d, 'save.atomic=1')
    assert started(vm)
    sox_action('save')
    sox_action('save')
    vm.stop()
    assert (d / 'app.sab.bak').exists()
    assert not list(d.glob('app.sab.bak.*'))


TESTS = [test_defaults, test_legacy_stage_unvalidated, test_validate_rejects,
         test_validate_truncated_sab, test_rollback_on_vm_error,
         test_rollback_after_crash_loop, test_trial_accept_and_restart_install,
         test_atomic_save, test_save_keep, test_save_keep_default]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', default='amd64')
    parser.add_argument('-k', help='run tests whose name contains this text')
    args = parser.parse_args()
    out = stage_out(args.arch)
    subprocess.run(['javac', '-nowarn', '-cp', str(HOME / 'lib/sedona.jar'), '-d', str(JAVA),
                    str(HERE / 'SoxAction.java')], check=True)
    sab = prepare_app(out)
    failed = 0
    for test in TESTS:
        if args.k and args.k not in test.__name__:
            continue
        t0 = time.time()
        try:
            test(out, sab)
            print('PASS %-40s %5.1fs' % (test.__name__, time.time() - t0), flush=True)
        except Exception as e:  # noqa: BLE001
            failed += 1
            print('FAIL %-40s %s' % (test.__name__, str(e)[-1500:]), flush=True)
        finally:
            for vm in list(Vm.alive):
                vm.stop()
    print('stage1: %d failed' % failed)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
