"""Kit widgets in the web editor, end to end on this PC (Linux):

* a loopback VM of the SDK's own Linux image with one unbuilt air handling unit (sproAhu::Ahu),
* the editor server on a test port with writes enabled,
* the API: the graph carries @widget, actions and range names; /api/kitui serves the device's
  ui.zip; an action is prepared (nothing written), confirmed (options written with readback, the
  action invoked, the tree read again); a stale plan is refused;
* with SEDONA_TEST_BROWSER (default: /usr/bin/google-chrome if present) the same in the browser:
  open the widget from the inspector, choose options, build, operation face, a setpoint written
  with confirmation; screenshots in build/tests/editor-widget/.

Run after ./spro build linux and the web/server builds:  python3 editor/tests/kit_widget_checks.py
Uses only 127.0.0.1 ports 21988 (SOX) and 28198 (HTTP); touches no running device."""
from pathlib import Path
import base64, hashlib, json, os, shutil, subprocess, sys, time, tomllib, urllib.request, urllib.error
import xml.etree.ElementTree as E

EDITOR = Path(__file__).resolve().parents[1]
ROOT = EDITOR.parent
SOX, HTTP = 21988, 28198
WORK = ROOT / 'build/tests/editor-widget'
NODE = os.environ.get('SEDONA_TEST_NODE', 'node')


def image():
    linux = tomllib.loads((ROOT / 'project.toml').read_text())['targets']['linux']
    return ROOT / 'build' / ('linux-amd64-%s-%s' % (linux.get('default_profile', 'full'), linux.get('default_board', 'sim')))


def prepare_vm():
    out = image()
    if WORK.exists():
        shutil.rmtree(WORK)
    (WORK / 'vm').mkdir(parents=True)
    vm = WORK / 'vm'
    for name in ('svm', 'kits.scode'):
        shutil.copy2(out / 'run' / name, vm / name)
    shutil.copytree(out / 'run/manifests', vm / 'manifests')
    kits = [d.get('on').split()[0] for d in E.parse(out / 'kits.xml').getroot().iter('depend')]
    assert 'sproAhu' in kits, 'the default image has no sproAhu'
    app = E.Element('sedonaApp')
    schema = E.SubElement(app, 'schema')
    for k in kits:
        E.SubElement(schema, 'kit', name=k)
    root = E.SubElement(app, 'app')
    E.SubElement(root, 'prop', name='appName', val='WebWidgetTest')
    svc = E.SubElement(root, 'comp', name='service', type='sys::Folder')
    E.SubElement(svc, 'comp', name='plat', type='sproPlatformLinux::UnixPlatformService')
    users = E.SubElement(svc, 'comp', name='users', type='sys::UserService')
    user = E.SubElement(users, 'comp', name='admin', type='sys::User')
    cred = base64.b64encode(hashlib.sha1(b'admin:localJarTest').digest()).decode()
    for n, v in (('cred', cred), ('perm', '2147483647'), ('prov', '255')):
        E.SubElement(user, 'prop', name=n, val=v)
    sox = E.SubElement(svc, 'comp', name='sox', type='sox::SoxService')
    E.SubElement(sox, 'prop', name='port', val=str(SOX))
    unit = E.SubElement(root, 'comp', name='unit', type='sproAhu::Ahu')
    for n, v in (('buildOnStart', 'false'), ('autoChannels', 'false')):
        E.SubElement(unit, 'prop', name=n, val=v)
    E.SubElement(app, 'links')
    E.ElementTree(app).write(vm / 'app.sax', encoding='utf-8')
    home = ROOT / 'build/home'
    cp = os.pathsep.join(str(home / 'lib' / j) for j in ('sedona.jar', 'sedonac.jar'))
    subprocess.run(['java', '-Dsedona.home=' + str(home), '-cp', cp, 'sedonac.Main', str(vm / 'app.sax')], check=True, capture_output=True)
    return vm


def api(path, body=None):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request('http://127.0.0.1:%d/api/%s' % (HTTP, path), data=data, method='GET' if body is None else 'POST',
                                 headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)


def refused(path, body, needle):
    try:
        api(path, body)
    except urllib.error.HTTPError as e:
        msg = json.load(e).get('error', '')
        assert needle in msg, msg
        return
    raise AssertionError(path + ' accepted')


def check_api():
    c = api('connect', dict(host='127.0.0.1', port=SOX, username='admin', password='localJarTest', mode='LIVE'))
    consumer, g = c['consumer'], c['graph']
    unit = next(n for n in g['nodes'] if n['type'] == 'sproAhu::Ahu')
    assert unit.get('widget') == 'ahu' and 'build' in unit['actions'], unit.get('actions')
    layout = next(p for p in unit['ports'] if p['name'] == 'layout')
    assert layout['range'] == ['supply', 'supplyExhaust'] and layout['config'], layout
    print('PASS graph: @widget, actions and range names of the Ahu')
    ui = api('kitui', dict(consumer=consumer))
    assert ui['source'] == 'device' and 'ahu.widget.json' in ui['kits']['sproAhu']['files'], ui.get('deviceProblem')
    print('PASS kitui: the device serves ui.zip (%d files of sproAhu)' % len(ui['kits']['sproAhu']['files']))
    uid = unit['id']
    refused('action/prepare', dict(consumer=consumer, componentId=uid, action='nosuch', values={}), 'No action')
    refused('action/prepare', dict(consumer=consumer, componentId=uid, action='build', values={'parts': '3'}), 'not a writable')
    refused('action/prepare', dict(consumer=consumer, componentId=0, action='save', values={}), 'kit widget')
    p = api('action/prepare', dict(consumer=consumer, componentId=uid, action='build', values={'layout': '1', 'recovery': '1'}))
    assert {c['slot'] for c in p['changes']} == {'layout', 'recovery'}
    g2 = api('graph')
    assert not any(n['parent'] == uid for n in g2['nodes'] if n['id'] != uid), 'prepare must not write or build'
    print('PASS prepare: plan with the two options, nothing written')
    r = api('action/commit', dict(consumer=consumer, token=p['token']))
    kids = [n for n in r['graph']['nodes'] if n['parent'] == uid and n['id'] != uid]
    assert r['status'] == 'DONE' and len(kids) >= 15 and any(n['name'] == 'rec' for n in kids), len(kids)
    refused('action/commit', dict(consumer=consumer, token=p['token']), 'missing')
    print('PASS commit: options written with readback, build invoked, %d parts read back; the token is single-use' % len(kids))
    # Rebuild with an electric heater: the device replaces a component under a reused id.
    p = api('action/prepare', dict(consumer=consumer, componentId=uid, action='build', values={'heater': '2'}))
    r = api('action/commit', dict(consumer=consumer, token=p['token']))
    heater = next(n for n in r['graph']['nodes'] if n['parent'] == uid and n['name'] == 'heater')
    assert heater['type'] == 'sproHvac::ElectricHeater', heater['type']
    print('PASS rebuild: water heater replaced by an electric one, tree read afresh')
    p = api('action/prepare', dict(consumer=consumer, componentId=uid, action='build', values={'heater': '1'}))
    api('action/commit', dict(consumer=consumer, token=p['token']))
    # A rebuild that removes parts: a lease renewal still carrying their ids (the browser's timer
    # took them from the old tree) leaves them out and keeps the session.
    old = [n['id'] for n in api('graph')['nodes'] if n['parent'] == uid and n['id'] != uid]
    p = api('action/prepare', dict(consumer=consumer, componentId=uid, action='build', values={'recovery': '0'}))
    r = api('action/commit', dict(consumer=consumer, token=p['token']))
    left = {n['id'] for n in r['graph']['nodes']}
    gone = [i for i in old if i not in left]
    assert gone, 'switching the recovery off removed nothing'
    s = api('watch', dict(consumer=consumer, ids=[int(str(i).lstrip('n')) for i in old + [uid]]))
    assert s['mode'] == 'LIVE' and s['subscriptions'] >= 1, s
    print('PASS renewal after a rebuild: %d removed parts left out of the lease, still LIVE' % len(gone))
    api('disconnect', {})


def main():
    for tool in ('java',):
        assert shutil.which(tool), tool + ' missing'
    if not (EDITOR / 'server/classes/WebServer.class').is_file() or not (EDITOR / 'web/dist/index.html').is_file():
        sys.exit('build the editor first: npm run build --prefix editor/web; python3 editor/tools/build_server.py')
    vm = prepare_vm()
    vm_log = open(WORK / 'svm.log', 'wb')
    vm_proc = subprocess.Popen(['./svm', 'kits.scode', 'app.sab'], cwd=vm, stdout=vm_log, stderr=subprocess.STDOUT, env=dict(os.environ, SVM_INET_BIND='127.0.0.1'))
    srv_log = open(WORK / 'server.log', 'wb')
    cp = os.pathsep.join([str(EDITOR / 'server/classes'), str(ROOT / 'build/home/lib/sedona.jar')])
    srv = subprocess.Popen(['java', '-Dsedona.home=' + str(ROOT / 'build/home'), '-Dsedona.allowWrites=true', '-Dport=%d' % HTTP, '-cp', cp, 'WebServer'],
                           cwd=EDITOR, stdout=srv_log, stderr=subprocess.STDOUT)
    try:
        for _ in range(60):
            try:
                api('status'); break
            except Exception:
                time.sleep(0.25)
        time.sleep(1)
        check_api()
        browser = os.environ.get('SEDONA_TEST_BROWSER', '/usr/bin/google-chrome' if Path('/usr/bin/google-chrome').exists() else '')
        if browser:
            env = dict(os.environ, SEDONA_TEST_URL='http://127.0.0.1:%d' % HTTP, SEDONA_TEST_SOX=str(SOX), SEDONA_TEST_BROWSER=browser, SEDONA_TEST_OUT=str(WORK))
            r = subprocess.run([NODE, str(EDITOR / 'tests/kit-widget-browser.mjs')], env=env, capture_output=True, text=True, timeout=240)
            print(r.stdout + r.stderr)
            assert r.returncode == 0, 'browser check failed'
        else:
            print('SKIP browser: no Chrome (set SEDONA_TEST_BROWSER)')
        print('PASS kit widgets in the web editor')
    finally:
        srv.terminate(); vm_proc.terminate()
        srv.wait(10); vm_proc.wait(10)


if __name__ == '__main__':
    main()
