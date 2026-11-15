# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Widgets shipped with kits (docs/widgets.md): check, install into the SDK,
pack into manifests/ui.zip for the controller.

    spro ui check        validate every kit's ui/ against the kit manifests
"""
import hashlib
import io
import json
import re
import shutil
import sys
import xml.etree.ElementTree as ET
import zipfile

from . import home as H
from . import project as P

FORMAT = 'sedonapro-widget/1'
UI_FORMAT = 'sedonapro-ui/1'
MAX_SVG = 64 * 1024
STATES = ('off', 'on', 'fault')
CONTROL_KINDS = ('mode', 'setpoint', 'toggle', 'status')
WRITE_KINDS = ('mode', 'setpoint', 'toggle')
NUMBER_TYPES = ('byte', 'short', 'int', 'long', 'float', 'double')


class WidgetError(Exception):
    pass


def ui_kits(known=None):
    """Kits with a ui/ directory."""
    known = known or P.kits()
    return {n: k for n, k in known.items() if (k.dir / 'ui').is_dir()}


# ---------------------------------------------------------------- manifests

class Types:
    """Types and slots from the compiled kit manifests (build/home/manifests)."""

    def __init__(self, kits):
        self.types = {}
        for kit in kits:
            files = sorted((H.HOME / 'manifests' / kit).glob(kit + '-*.xml'), key=lambda p: p.stat().st_mtime)
            if not files:
                continue
            root = ET.parse(files[-1]).getroot()
            for t in root.findall('type'):
                slots = {}
                for s in t.findall('slot'):
                    facets = {f.get('name'): f.get('val') for f in s.findall('facets/*')}
                    slots[s.get('name')] = {'type': s.get('type'), 'flags': s.get('flags') or '',
                                            'range': facets.get('range')}
                tf = {f.get('name'): f.get('val') for f in t.findall('facets/*')}
                self.types['%s::%s' % (kit, t.get('name'))] = {'base': t.get('base'), 'slots': slots, 'facets': tf}

    def slot(self, qname, name):
        seen = set()
        while qname and qname not in seen:
            seen.add(qname)
            t = self.types.get(qname)
            if t is None:
                return None
            if name in t['slots']:
                return t['slots'][name]
            qname = t['base']
        return None

    def facet(self, qname, name):
        t = self.types.get(qname)
        return t['facets'].get(name) if t else None


# ---------------------------------------------------------------- conditions

COND = re.compile(r"^\s*(\w+)\s*(==|!=|<=|>=|<|>)\s*('[\w-]*'|-?\d+(\.\d+)?|true|false)\s*$")


def check_condition(expr, own_slot, where):
    """own_slot(name) -> slot dict or None"""
    if expr is None or expr.strip() in ('', 'true'):
        return
    for alt in expr.split('||'):
        for clause in alt.split('&&'):
            m = COND.match(clause)
            if not m:
                raise WidgetError('%s: bad condition %r' % (where, clause.strip()))
            slot = own_slot(m.group(1))
            if slot is None:
                raise WidgetError('%s: condition on unknown slot %r' % (where, m.group(1)))
            value = m.group(3)
            if value.startswith("'"):
                names = [x.strip() for x in (slot['range'] or '').split(',')]
                if value.strip("'") not in names:
                    raise WidgetError('%s: %r is not in the range of %s (%s)' % (
                        where, value, m.group(1), ', '.join(names) or 'no range'))


# ---------------------------------------------------------------- svg

BAD_SVG = [
    (re.compile(r'<\s*script', re.I), '<script>'),
    (re.compile(r'<\s*foreignObject', re.I), '<foreignObject>'),
    (re.compile(r'<\s*image\b', re.I), '<image>'),
    (re.compile(r'\son\w+\s*=', re.I), 'an on... event attribute'),
    (re.compile(r'javascript:', re.I), 'javascript:'),
    (re.compile(r'href\s*=\s*["\'](?!#)', re.I), 'a link outside the file'),
    (re.compile(r'url\(\s*["\']?(?!#)', re.I), 'url() outside the file'),
    (re.compile(r'@import', re.I), '@import'),
    (re.compile(r'<!ENTITY', re.I), 'an XML entity'),
]


# The SVG subset both editors draw (the web editor as SVG, Sedona Studio as
# JavaFX shapes): simple shapes, groups, text, a rotation animation.
SVG_COMMON = {'id', 'fill', 'stroke', 'stroke-width', 'opacity', 'fill-opacity',
              'stroke-opacity', 'stroke-linecap', 'stroke-linejoin',
              'stroke-dasharray', 'transform'}
SVG_ELEMENTS = {
    'svg': {'xmlns', 'viewBox', 'width', 'height', 'version', 'preserveAspectRatio'},
    'g': set(),
    'title': set(), 'desc': set(),
    'path': {'d'},
    'rect': {'x', 'y', 'width', 'height', 'rx', 'ry'},
    'circle': {'cx', 'cy', 'r'},
    'ellipse': {'cx', 'cy', 'rx', 'ry'},
    'line': {'x1', 'y1', 'x2', 'y2'},
    'polyline': {'points'},
    'polygon': {'points'},
    'text': {'x', 'y', 'font-size', 'font-family', 'font-weight', 'text-anchor'},
    'animateTransform': {'attributeName', 'type', 'from', 'to', 'dur', 'repeatCount'},
}
SVG_PAINT = re.compile(r'^(none|#[0-9a-fA-F]{3}|#[0-9a-fA-F]{6}|black|white|red|green|blue|yellow|orange|gray|grey)$')
SVG_TRANSFORM = re.compile(r'^(\s*(translate|rotate|scale|matrix)\(\s*-?[\d.]+([\s,]+-?[\d.]+)*\s*\)\s*)+$')


def check_svg(path):
    data = path.read_bytes()
    if len(data) > MAX_SVG:
        raise WidgetError('%s: %d bytes, the limit is %d' % (path, len(data), MAX_SVG))
    text = data.decode('utf-8')
    for rx, what in BAD_SVG:
        if rx.search(text):
            raise WidgetError('%s: %s is not allowed' % (path, what))
    try:
        root = ET.fromstring(text)
    except ET.ParseError as e:
        raise WidgetError('%s: not XML: %s' % (path, e))
    if not root.tag.endswith('svg'):
        raise WidgetError('%s: root is not <svg>' % path)
    if not root.get('viewBox'):
        raise WidgetError('%s: <svg> needs a viewBox' % path)
    for e in root.iter():
        tag = e.tag.split('}')[-1]
        if tag not in SVG_ELEMENTS:
            raise WidgetError('%s: <%s> is not in the SVG subset (docs/widgets.md)' % (path, tag))
        for a, v in e.attrib.items():
            name = a.split('}')[-1]
            if name not in SVG_ELEMENTS[tag] and name not in SVG_COMMON:
                raise WidgetError('%s: <%s %s> is not in the SVG subset' % (path, tag, name))
            if name in ('fill', 'stroke') and not SVG_PAINT.match(v):
                raise WidgetError('%s: <%s %s="%s">: colours are #rgb, #rrggbb, none or a basic name' % (path, tag, name, v))
            if name == 'transform' and not SVG_TRANSFORM.match(v):
                raise WidgetError('%s: <%s transform="%s"> is not supported' % (path, tag, v))
        if tag == 'svg' and e.get('preserveAspectRatio', 'none') not in ('none', 'xMidYMid meet', 'xMidYMid'):
            raise WidgetError('%s: preserveAspectRatio is "none" or "xMidYMid meet"' % path)
        if tag == 'animateTransform' and (e.get('attributeName') != 'transform' or e.get('type') != 'rotate'):
            raise WidgetError('%s: only a rotating animateTransform is supported' % path)


# ---------------------------------------------------------------- widgets

def need(d, key, kind, where):
    if key not in d or not isinstance(d[key], kind):
        raise WidgetError('%s: %s missing or of the wrong kind' % (where, key))
    return d[key]


def check_widget(kit, path, types):
    """Validate one <id>.widget.json; returns its index entry."""
    where = '%s/ui/%s' % (kit.rel, path.name)
    try:
        w = json.loads(path.read_text(encoding='utf-8'))
    except ValueError as e:
        raise WidgetError('%s: not JSON: %s' % (where, e))
    if w.get('format') != FORMAT:
        raise WidgetError('%s: format must be %r' % (where, FORMAT))
    wid = need(w, 'id', str, where)
    if path.name != wid + '.widget.json' or not re.fullmatch(r'[a-z][a-zA-Z0-9-]*', wid):
        raise WidgetError('%s: id %r must match the file name <id>.widget.json' % (where, wid))
    qtype = need(w, 'type', str, where)
    if qtype not in types.types:
        raise WidgetError('%s: unknown type %s (compile the kit first)' % (where, qtype))
    if types.facet(qtype, 'widget') != wid:
        raise WidgetError('%s: type %s needs the facet @widget="%s"' % (where, qtype, wid))
    need(w, 'title', str, where)
    size = need(w, 'size', list, where)
    if len(size) != 2 or not all(isinstance(x, int) and 100 <= x <= 4000 for x in size):
        raise WidgetError('%s: size must be [width, height] in 100..4000' % where)

    parts = w.get('parts', {})
    for name, tlist in parts.items():
        if not isinstance(tlist, list) or not tlist:
            raise WidgetError('%s: parts.%s must list types' % (where, name))
        for t in tlist:
            if t not in types.types:
                raise WidgetError('%s: parts.%s: unknown type %s' % (where, name, t))

    def own(name):
        return types.slot(qtype, name)

    def bound(ref, ctx, writable=False, kinds=None):
        if not isinstance(ref, str) or not ref:
            raise WidgetError('%s: %s: binding missing' % (where, ctx))
        if '.' in ref:
            part, slot = ref.split('.', 1)
            if part not in parts:
                raise WidgetError('%s: %s: %r is not in parts' % (where, ctx, part))
            found = [types.slot(t, slot) for t in parts[part]]
            found = [s for s in found if s]
            if not found:
                raise WidgetError('%s: %s: no slot %r in %s' % (where, ctx, slot, ', '.join(parts[part])))
        else:
            s = own(ref)
            if s is None:
                raise WidgetError('%s: %s: no slot %r in %s' % (where, ctx, ref, qtype))
            found = [s]
        for s in found:
            if writable and 'c' not in s['flags']:
                raise WidgetError('%s: %s: %s is not a @config slot' % (where, ctx, ref))
            if kinds and s['type'] not in kinds:
                raise WidgetError('%s: %s: %s is %s, expected %s' % (where, ctx, ref, s['type'], '/'.join(kinds)))
        return found[0]

    symbols = w.get('symbols', {})
    ui = kit.dir / 'ui'
    files = set()
    for sid, states in symbols.items():
        if not isinstance(states, dict) or 'off' not in states:
            raise WidgetError('%s: symbol %s needs at least the "off" state' % (where, sid))
        for st, rel in states.items():
            if st not in STATES:
                raise WidgetError('%s: symbol %s: unknown state %r' % (where, sid, st))
            f = (ui / rel).resolve()
            if ui.resolve() not in f.parents or not f.is_file() or f.suffix != '.svg':
                raise WidgetError('%s: symbol %s: no file ui/%s' % (where, sid, rel))
            check_svg(f)
            files.add(rel)

    for i, o in enumerate(w.get('options', [])):
        bound(o.get('slot'), 'options[%d]' % i, writable=True)
        need(o, 'label', str, '%s: options[%d]' % (where, i))
        if 'when' in o:
            check_condition(o['when'], own, '%s: options[%d].when' % (where, i))
    option_slots = {o['slot'] for o in w.get('options', [])}

    ids = set()
    for i, e in enumerate(need(w, 'elements', list, where)):
        ctx = 'elements[%d]' % i
        eid = need(e, 'id', str, where + ': ' + ctx)
        if eid in ids:
            raise WidgetError('%s: %s: duplicate id %s' % (where, ctx, eid))
        ids.add(eid)
        if e.get('symbol') not in symbols:
            raise WidgetError('%s: %s: unknown symbol %r' % (where, ctx, e.get('symbol')))
        for k in ('x', 'y', 'w', 'h'):
            v = e.get(k)
            if not isinstance(v, (int, float)) or v < 0 or v > 4000:
                raise WidgetError('%s: %s: %s must be a number 0..4000' % (where, ctx, k))
        check_condition(e.get('when'), own, '%s: %s.when' % (where, ctx))
        if not isinstance(e.get('ghost', False), bool):
            raise WidgetError('%s: %s.ghost must be true or false' % (where, ctx))
        if 'state' in e:
            bound(e['state'], ctx + '.state', kinds=('bool',) + NUMBER_TYPES)
        if 'fault' in e:
            bound(e['fault'], ctx + '.fault', kinds=('bool',))
        for c in e.get('configure', []):
            if c not in option_slots:
                raise WidgetError('%s: %s.configure: %r is not in options' % (where, ctx, c))
        for j, v in enumerate(e.get('values', [])):
            bound(v.get('bind'), '%s.values[%d]' % (ctx, j))
            check_condition(v.get('when'), own, '%s: %s.values[%d].when' % (where, ctx, j))

    for i, c in enumerate(w.get('controls', [])):
        ctx = 'controls[%d]' % i
        kind = c.get('kind')
        if kind not in CONTROL_KINDS:
            raise WidgetError('%s: %s: kind must be one of %s' % (where, ctx, ', '.join(CONTROL_KINDS)))
        s = bound(c.get('bind'), ctx, writable=kind in WRITE_KINDS,
                  kinds={'mode': ('byte', 'int'), 'setpoint': NUMBER_TYPES, 'toggle': ('bool',)}.get(kind))
        if kind == 'mode' and not s['range']:
            raise WidgetError('%s: %s: a mode needs a slot with a range' % (where, ctx))
        check_condition(c.get('when'), own, '%s: %s.when' % (where, ctx))
    for i, x in enumerate(w.get('info', [])):
        bound(x.get('bind'), 'info[%d]' % i)
    for i, a in enumerate(w.get('actions', [])):
        ctx = 'actions[%d]' % i
        s = own(a.get('invoke') or '')
        if s is None or 'a' not in s['flags']:
            raise WidgetError('%s: %s: %r is not an action of %s' % (where, ctx, a.get('invoke'), qtype))
        if not a.get('confirm'):
            raise WidgetError('%s: %s: confirm text is required' % (where, ctx))
    for i, t in enumerate(w.get('texts', [])):
        need(t, 'text', str, '%s: texts[%d]' % (where, i))
        check_condition(t.get('when'), own, '%s: texts[%d].when' % (where, i))
    return {'id': wid, 'type': qtype, 'file': path.name, 'symbols': sorted(files)}


def check_kit(kit, types):
    ui = kit.dir / 'ui'
    widgets = [check_widget(kit, p, types) for p in sorted(ui.glob('*.widget.json'))]
    if not widgets:
        raise WidgetError('%s/ui: no *.widget.json' % kit.rel)
    for p in sorted(ui.glob('texts.*.json')):
        try:
            texts = json.loads(p.read_text(encoding='utf-8'))
        except ValueError as e:
            raise WidgetError('%s: not JSON: %s' % (p, e))
        if not isinstance(texts, dict) or not all(isinstance(k, str) and isinstance(v, str) for k, v in texts.items()):
            raise WidgetError('%s: must map English text to text' % p)
    for p in sorted(ui.rglob('*.svg')):
        check_svg(p)
    # every type with @widget has its widget
    have = {w['id'] for w in widgets}
    for q, t in types.types.items():
        if q.startswith(kit.name + '::') and t['facets'].get('widget') and t['facets']['widget'] not in have:
            raise WidgetError('%s: %s has @widget="%s" but ui/%s.widget.json is missing' % (
                kit.rel, q, t['facets']['widget'], t['facets']['widget']))
    return widgets


def check(names=None, compile=True):
    known = P.kits()
    kits = ui_kits(known)
    if names:
        kits = {n: k for n, k in kits.items() if n in names}
    if compile:
        H.compile_kits(sorted(kits))
    types = Types(P.closure(sorted(kits), known))
    out = {}
    for name, kit in sorted(kits.items()):
        out[name] = check_kit(kit, types)
    return out


def cmd_check():
    try:
        result = check()
    except WidgetError as e:
        sys.exit('ui check: %s' % e)
    for kit, widgets in result.items():
        print('%-14s %s' % (kit, ', '.join('%s (%s)' % (w['id'], w['type']) for w in widgets)))
    print('ui check: %d kits, %d widgets OK' % (len(result), sum(len(w) for w in result.values())))
    return 0


# ---------------------------------------------------------------- packing

def pack(kit_names, scode_kits, out_zip):
    """manifests/ui.zip with the ui/ of the kits in the image; None if there is none.
    scode_kits: [(name, checksum, version)] of the image."""
    known = P.kits()
    kits = {n: k for n, k in ui_kits(known).items() if n in kit_names}
    if not kits:
        if out_zip.exists():
            out_zip.unlink()
        return None
    widgets = check(sorted(kits), compile=False)
    info = {n: (c, v) for n, c, v in scode_kits}
    index = {'format': UI_FORMAT, 'kits': {}}
    entries = []
    for name, kit in sorted(kits.items()):
        chk, ver = info.get(name, (0, ''))
        index['kits'][name] = {'version': ver, 'checksum': '%08x' % chk,
                               'widgets': [{k: w[k] for k in ('id', 'type', 'file')} for w in widgets[name]]}
        for f in sorted((kit.dir / 'ui').rglob('*')):
            if f.is_file():
                entries.append(('%s/%s' % (name, f.relative_to(kit.dir / 'ui').as_posix()), f.read_bytes()))
    entries.insert(0, ('index.json', json.dumps(index, indent=1, sort_keys=True).encode()))
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in entries:
            zi = zipfile.ZipInfo(name, (2020, 1, 1, 0, 0, 0))
            zi.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(zi, data)
    out_zip.write_bytes(buf.getvalue())
    digest = hashlib.sha256(buf.getvalue()).hexdigest()[:12]
    print('ui.zip: %d kits, %d widgets, %d bytes (%s)' % (
        len(kits), sum(len(w) for w in widgets.values()), out_zip.stat().st_size, digest))
    return out_zip


def install():
    """Copy every kit's ui/ into the SDK: build/home/ui/<kit>/"""
    dest = H.HOME / 'ui'
    for name, kit in ui_kits().items():
        d = dest / name
        if d.exists():
            shutil.rmtree(d)
        shutil.copytree(kit.dir / 'ui', d)
