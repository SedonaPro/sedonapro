#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Linux VM smoke test: start the built svm in a copy of its run directory and
check the platform start, web server and (board sim) the I/O demo through the
GPIO panel API.  usage: smoke_test.py <build/linux-...>/run
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import urllib.request
import xml.etree.ElementTree as ET

GPIO_PORT = 18091


ROOT = Path(__file__).resolve().parents[3]
# the app runs on test ports, so a controller or simulator already running on
# this machine (SOX 1876, web 8080, Modbus 1502 ...) is neither disturbed nor
# mistaken for the VM under test
PORTS = {'1876': '21976', '8080': '28180', '8088': '28188', '1502': '21602'}
SOX, WEB = PORTS['1876'], PORTS['8080']
HOME = ROOT / 'build/home'
JAVA_OUT = ROOT / 'build/tests-java'


def test_app(run, work):
    """work/app.sab: the built app (run/../app.sax) with the test ports."""
    tree = ET.parse(run.parent / 'app.sax')
    for comp in tree.getroot().iter('comp'):
        port = comp.find("prop[@name='port']")
        if comp.get('type') == 'sox::SoxService' and port is None:
            port = ET.SubElement(comp, 'prop', name='port', val='1876')
        if port is not None and port.get('val') in PORTS:
            port.set('val', PORTS[port.get('val')])
    sax = work / 'app-test.sax'
    tree.write(sax, encoding='unicode')
    cp = os.pathsep.join(str(HOME / 'lib' / j) for j in ('sedona.jar', 'sedonac.jar'))
    r = subprocess.run(['java', '-Dsedona.home=' + str(HOME), '-cp', cp, 'sedonac.Main', str(sax)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout[-1500:]
    shutil.move(str(sax.with_suffix('.sab')), str(work / 'app.sab'))


def files(run, work):
    """m:ui.zip is served and equals the image; names leaving manifests/ are refused."""
    JAVA_OUT.mkdir(parents=True, exist_ok=True)
    src = ROOT / 'targets/esp32/tests/SoxProbe.java'
    if not (JAVA_OUT / 'SoxProbe.class').exists() or (JAVA_OUT / 'SoxProbe.class').stat().st_mtime < src.stat().st_mtime:
        subprocess.run(['javac', '-nowarn', '-cp', str(HOME / 'lib/sedona.jar'), '-d', str(JAVA_OUT), str(src)], check=True)
    ui = work / 'manifests/ui.zip'
    props = ['-Dprobe.ui=' + str(ui)] if ui.exists() else []
    r = subprocess.run(['java', '-Dsedona.home=' + str(HOME), *props, '-cp',
                        os.pathsep.join([str(HOME / 'lib/sedona.jar'), str(JAVA_OUT)]),
                        'SoxProbe', '127.0.0.1', SOX, 'files'],
                       env=dict(os.environ, SEDONA_ADMIN_PASSWORD=os.environ.get('SEDONA_ADMIN_PASSWORD', 'sedonapro-dev')),
                       capture_output=True, text=True, timeout=60)
    assert 'PROBE files OK' in r.stdout, r.stdout + r.stderr[-800:]
    return 'm:ui.zip %d bytes, traversal refused' % ui.stat().st_size if ui.exists() else 'traversal refused'


def main(run):
    run = Path(run).resolve()
    work = run.parent / 'smoke'
    if work.exists():
        shutil.rmtree(work)
    shutil.copytree(run, work)
    test_app(run, work)
    log = (work / 'svm.log').open('w')
    proc = subprocess.Popen([str(work / 'svm'), '--plat'], cwd=work, stdout=log, stderr=subprocess.STDOUT,
                            env=dict(os.environ, SPRO_GPIO_PORT=str(GPIO_PORT)))
    text = lambda: (work / 'svm.log').read_text(errors='replace')
    notes = []
    try:
        end = time.time() + 30
        while '[sys::App] running' not in text() or 'Failed to bind' in text():
            assert proc.poll() is None and time.time() < end, text()[-2000:]
            time.sleep(0.2)
        notes.append('platform running')
        body = urllib.request.urlopen('http://127.0.0.1:%s/' % WEB, timeout=5).read().decode(errors='replace')
        assert 'Sedona' in body or 'sedona' in body, body[:200]
        notes.append('web ok')
        notes.append(files(run, work))
        if 'GPIO panel' in text():
            def api(path):
                with urllib.request.urlopen('http://127.0.0.1:%d%s' % (GPIO_PORT, path), timeout=5) as r:
                    return json.loads(r.read().decode())

            def val(name):
                return next(c['value'] for c in api('/api/state')['channels'] if c['name'] == name)

            def wait(cond):
                t = time.time() + 5
                while not cond():
                    assert time.time() < t
                    time.sleep(0.1)
            api('/api/set?ch=DI1&v=1')
            wait(lambda: val('DO1') == 1)
            api('/api/set?ch=AI1&v=5')                 # 5 V of 0..10 V
            wait(lambda: abs(val('AO1') - 5.0) < 0.01)
            notes.append('hal DI1->DO1, AI1->AO1')
        print('PASS linux smoke: ' + ', '.join(notes))
        return 0
    except AssertionError as e:
        print('FAIL linux smoke: %s\n%s' % (e, text()[-2000:]))
        return 1
    finally:
        proc.terminate()
        try:
            proc.wait(10)
        except subprocess.TimeoutExpired:
            proc.kill()
        log.close()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
