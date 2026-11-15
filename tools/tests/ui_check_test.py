# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""spro ui check must reject broken widgets: each case spoils a copy of the
sproAhu widget and expects a WidgetError.  Run after ./spro ui check (needs
the compiled manifests):  python3 tools/tests/ui_check_test.py"""
import copy
import json
import shutil
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from spro import project as P, ui  # noqa: E402

kit = P.kits()['sproAhu']
types = ui.Types(P.closure(['sproAhu'], P.kits()))
base = json.loads((kit.dir / 'ui/ahu.widget.json').read_text())


def run(mutate, svg=None):
    with tempfile.TemporaryDirectory() as d:
        d = Path(d)
        shutil.copytree(kit.dir / 'ui', d / 'ui')
        w = copy.deepcopy(base)
        mutate(w)
        (d / 'ui/ahu.widget.json').write_text(json.dumps(w))
        if svg:
            (d / 'ui/svg/fan-on.svg').write_text(svg)
        fake = SimpleNamespace(dir=d, rel=Path('kits/hvac/sproAhu'), name='sproAhu')
        ui.check_widget(fake, d / 'ui/ahu.widget.json', types)


def el(w, eid):
    return next(e for e in w['elements'] if e['id'] == eid)


CASES = {
    'unchanged': (lambda w: None, None, False),
    'unknown slot': (lambda w: el(w, 'fanS').__setitem__('state', 'doFanS.nope'), None, True),
    'unknown part': (lambda w: el(w, 'fanS').__setitem__('fault', 'ghost.alarm'), None, True),
    'fault not bool': (lambda w: el(w, 'fanS').__setitem__('fault', 'fanS.out'), None, True),
    'bad range name': (lambda w: el(w, 'cooler').__setitem__('when', "cooler != 'ice'"), None, True),
    'condition on unknown slot': (lambda w: el(w, 'cooler').__setitem__('when', "colour == 1"), None, True),
    'option not config': (lambda w: w['options'].append({'slot': 'missing', 'label': 'x'}), None, True),
    'write to readonly': (lambda w: w['controls'].append({'kind': 'setpoint', 'bind': 'temp.supplySp', 'label': 'x', 'x': 1, 'y': 1}), None, True),
    'action unknown': (lambda w: w['actions'].append({'id': 'x', 'invoke': 'reset', 'label': 'x', 'confirm': 'x'}), None, True),
    'action without confirm': (lambda w: w['actions'][0].pop('confirm'), None, True),
    'configure not an option': (lambda w: el(w, 'fanS')['configure'].append('nope'), None, True),
    'wrong type': (lambda w: w.__setitem__('type', 'sproHvac::Season'), None, True),
    'svg script': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg"><script>alert(1)</script></svg>', True),
    'svg handler': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg"><rect onclick="x()"/></svg>', True),
    'svg external link': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg"><use href="http://e/x.svg#a"/></svg>', True),
    'svg image': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg"><image href="#a"/></svg>', True),
    'svg style element': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><style>rect{fill:red}</style></svg>', True),
    'svg style attribute': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><rect style="fill:red"/></svg>', True),
    'svg gradient paint': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><rect fill="url(#g)"/></svg>', True),
    'svg without viewBox': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg"><rect/></svg>', True),
    'svg skew transform': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><rect transform="skewX(10)"/></svg>', True),
    'svg subset ok': (lambda w: None, '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><g transform="translate(1,2) rotate(45 5 5)"><rect x="1" y="1" width="2" height="2" fill="#abc"/></g></svg>', False),
}

failed = 0
for name, (mutate, svg, bad) in CASES.items():
    try:
        run(mutate, svg)
        ok = not bad
        msg = 'accepted'
    except ui.WidgetError as e:
        ok = bad
        msg = str(e).split(': ', 1)[-1]
    print('%s %-28s %s' % ('PASS' if ok else 'FAIL', name, msg))
    failed += not ok
print('ui check test: %d failed' % failed)
sys.exit(1 if failed else 0)
