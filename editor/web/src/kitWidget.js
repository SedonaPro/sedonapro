// Kit widgets (docs/widgets.md): templates and symbols shipped with kits. Everything here is data
// from a device, so it is parsed with limits, checked against the device's component and rebuilt
// from a whitelist; nothing is evaluated or inserted as markup.

export const FORMAT = 'sedonapro-widget/1';
const NAME = /^[A-Za-z_][A-Za-z0-9_]{0,63}$/;
const FILE = /^[A-Za-z0-9_-]+(\/[A-Za-z0-9_.-]+)*\.svg$/;
const STATES = ['off', 'on', 'fault'];
const CONTROLS = ['mode', 'setpoint', 'toggle', 'status'];
const MAX = 256;

function fail(msg) { throw new Error(msg); }
const str = (v, max = 1024) => (typeof v === 'string' && v.length <= max ? v : fail('Ожидается текст до ' + max + ' символов'));
const opt = (m, k) => (m[k] === undefined ? '' : str(m[k]));
const num = (v, lo, hi, d = 0) => { const x = v === undefined ? d : v; if (typeof x !== 'number' || !Number.isFinite(x) || x < lo || x > hi) fail('Число вне диапазона ' + lo + '..' + hi); return x; };
const name = s => (NAME.test(s) ? s : fail('Недопустимое имя ' + s));
const list = v => (v === undefined ? [] : Array.isArray(v) && v.length <= MAX ? v : fail('Ожидается список'));
const obj = v => (v && typeof v === 'object' && !Array.isArray(v) && Object.keys(v).length <= MAX ? v : fail('Ожидается объект'));

// ---------------------------------------------------------------- conditions

const TERM = /^\s*(\w+)\s*(==|!=|<=|>=|<|>)\s*('[\w-]*'|-?\d+(\.\d+)?|true|false)\s*$/;

/** "slot op value" terms joined by && and || (&& binds tighter). */
export function parseCondition(expr) {
  const e = (expr ?? '').trim();
  if (e === '' || e === 'true') return [];
  if (e.length > 512) fail('Слишком длинное условие');
  return e.split('||').map(or => or.split('&&').map(and => {
    const m = TERM.exec(and);
    if (!m) fail("Условие '" + expr + "'");
    return { slot: m[1], op: m[2], value: m[3] };
  }));
}

export function number(v) {
  if (v === undefined || v === null) return NaN;
  const s = String(v).trim();
  if (s === 'true') return 1;
  if (s === 'false') return 0;
  return s === '' ? NaN : Number(s);
}

/** ports: the component's ports by name (with .range); values: its values by slot name. */
export function testCondition(parsed, ports, values) {
  if (!parsed.length) return true;
  return parsed.some(all => all.every(t => {
    const left = number(values[t.slot]);
    const right = t.value.startsWith("'") ? (ports[t.slot]?.range ?? []).indexOf(t.value.slice(1, -1)) : number(t.value);
    if (Number.isNaN(left) || Number.isNaN(right) || right < 0 && t.value.startsWith("'")) return t.op === '!=';
    switch (t.op) { case '==': return left === right; case '!=': return left !== right; case '<': return left < right; case '>': return left > right; case '<=': return left <= right; default: return left >= right; }
  }));
}

// ---------------------------------------------------------------- templates

export function parseTemplate(text) {
  if (typeof text !== 'string' || text.length > 256 * 1024) fail('Шаблон слишком велик');
  const m = obj(JSON.parse(text));
  if (m.format !== FORMAT) fail('Неизвестный формат виджета ' + m.format);
  const size = list(m.size);
  if (size.length !== 2) fail('size: [ширина, высота]');
  const parts = {};
  for (const [k, v] of Object.entries(obj(m.parts ?? {}))) parts[name(k)] = list(v).map(t => str(t, 160));
  const bind = ref => { const [p, s, extra] = str(ref, 200).split('.'); if (extra !== undefined) fail('Привязка ' + ref); if (s === undefined) return name(p); name(s); if (!parts[name(p)]) fail(p + ' нет в parts'); return ref; };
  const cond = x => { const w = opt(x, 'when'); parseCondition(w); return w; };
  const symbols = {};
  for (const [k, v] of Object.entries(obj(m.symbols ?? {}))) {
    const states = {};
    for (const [st, f] of Object.entries(obj(v))) { if (!STATES.includes(st)) fail('Состояние ' + st); if (!FILE.test(str(f, 200)) || f.includes('..')) fail('Файл ' + f); states[st] = f; }
    if (!states.off) fail('Значку ' + k + ' нужно состояние off');
    symbols[name(k)] = states;
  }
  const t = {
    id: name(str(m.id)), type: str(m.type, 160), title: opt(m, 'title'),
    width: num(size[0], 50, 4000), height: num(size[1], 50, 4000), parts, symbols,
    options: list(m.options).map(o => ({ slot: name(str(obj(o).slot)), label: opt(o, 'label'), when: cond(o) })),
    elements: list(m.elements).map(x => {
      obj(x);
      if (x.symbol !== undefined && !symbols[x.symbol]) fail('Неизвестный значок ' + x.symbol);
      return {
        id: name(str(x.id)), symbol: opt(x, 'symbol'), x: num(x.x, -4000, 4000), y: num(x.y, -4000, 4000),
        w: num(x.w, 1, 4000, 40), h: num(x.h, 1, 4000, 40), when: cond(x), ghost: x.ghost === true,
        state: x.state === undefined ? '' : bind(x.state), fault: x.fault === undefined ? '' : bind(x.fault),
        label: opt(x, 'label'), configure: list(x.configure).map(c => name(str(c))),
        values: list(x.values).map(v => ({ bind: bind(obj(v).bind), unit: opt(v, 'unit'), decimals: num(v.decimals, 0, 6, 1), dx: num(v.dx, -4000, 4000), dy: num(v.dy, -4000, 4000), label: opt(v, 'label'), when: cond(v) })),
      };
    }),
    texts: list(m.texts).map(x => ({ text: str(obj(x).text), x: num(x.x, -4000, 4000), y: num(x.y, -4000, 4000), when: cond(x) })),
    controls: list(m.controls).map(x => { const kind = str(obj(x).kind); if (!CONTROLS.includes(kind)) fail('Элемент управления ' + kind); return { kind, bind: bind(x.bind), label: opt(x, 'label'), x: num(x.x, -4000, 4000), y: num(x.y, -4000, 4000), when: cond(x), unit: opt(x, 'unit') }; }),
    info: list(m.info).map(x => ({ bind: bind(obj(x).bind), label: opt(x, 'label') })),
    actions: list(m.actions).map(x => ({ id: name(str(obj(x).id)), invoke: name(str(x.invoke)), label: opt(x, 'label'), confirm: opt(x, 'confirm') })),
  };
  return t;
}

/** The children of a component the template names, by part name (only allowed types). */
export function partsOf(t, graph, compId) {
  const out = {};
  const id = String(compId).replace(/^n/, '');
  for (const n of graph.nodes) if (String(n.parent) === id && n.id !== compId && t.parts[n.name]?.includes(n.type)) out[n.name] = n;
  return out;
}

const portsOf = n => Object.fromEntries((n?.ports ?? []).map(p => [p.name, p]));

/** The block and port a binding reads, or null. */
export function target(t, comp, parts, ref) {
  const [p, s] = ref.includes('.') ? ref.split('.') : [null, ref];
  const node = p ? parts[p] : comp;
  const port = node && portsOf(node)[s];
  return port ? { node, port } : null;
}

/**
 * What does not match the device: the component's type and @widget, own slots, @config options and
 * controls, actions, condition slots and range names; a child with only one allowed type must have
 * the bound slots (a part with variants binds slots of each variant, shown as "—" on the others).
 */
export function problems(t, comp, parts) {
  const out = [];
  if (comp.type !== t.type) return ['тип ' + comp.type + ' ≠ ' + t.type];
  if (comp.widget !== t.id) out.push(t.type + ' не указывает на виджет ' + t.id);
  const own = portsOf(comp);
  const refs = [];
  for (const e of t.elements) { if (e.state) refs.push(e.state); if (e.fault) refs.push(e.fault); for (const v of e.values) refs.push(v.bind); }
  for (const c of t.controls) refs.push(c.bind);
  for (const i of t.info) refs.push(i.bind);
  for (const r of refs) {
    if (r.includes('.')) { const [p, s] = r.split('.'); if (parts[p] && t.parts[p].length === 1 && !portsOf(parts[p])[s]) out.push('нет слота ' + r); }
    else if (!own[r]) out.push('нет слота ' + r);
  }
  for (const o of t.options) if (!own[o.slot]?.config || own[o.slot]?.readonly) out.push('опция ' + o.slot + ' не @config');
  for (const c of t.controls) if (c.kind !== 'status') { const x = target(t, comp, parts, c.bind); if (x && !x.port.config) out.push('элемент ' + c.bind + ' не @config'); }
  for (const e of t.elements) for (const o of e.configure) if (!t.options.some(x => x.slot === o)) out.push(e.id + ' настраивает ' + o + ', которой нет в опциях');
  for (const a of t.actions) { if (!(comp.actions ?? []).includes(a.invoke)) out.push('нет действия ' + a.invoke); if (!a.confirm.trim()) out.push('у действия ' + a.id + ' нет текста подтверждения'); }
  const conds = [...t.elements.flatMap(e => [e.when, ...e.values.map(v => v.when)]), ...t.texts.map(x => x.when), ...t.controls.map(x => x.when), ...t.options.map(x => x.when)];
  for (const w of conds) for (const all of parseCondition(w)) for (const term of all) {
    if (!own[term.slot]) out.push('условие по неизвестному слоту ' + term.slot);
    else if (term.value.startsWith("'") && !(own[term.slot].range ?? []).includes(term.value.slice(1, -1))) out.push('условие: ' + term.value + ' нет в ' + term.slot);
  }
  return out;
}

// ---------------------------------------------------------------- SVG subset

const COMMON = ['id', 'fill', 'stroke', 'stroke-width', 'opacity', 'fill-opacity', 'stroke-opacity', 'stroke-linecap', 'stroke-linejoin', 'stroke-dasharray', 'transform'];
export const SVG_ELEMENTS = {
  svg: ['xmlns', 'viewBox', 'width', 'height', 'version', 'preserveAspectRatio'], g: [], title: [], desc: [], path: ['d'],
  rect: ['x', 'y', 'width', 'height', 'rx', 'ry'], circle: ['cx', 'cy', 'r'], ellipse: ['cx', 'cy', 'rx', 'ry'],
  line: ['x1', 'y1', 'x2', 'y2'], polyline: ['points'], polygon: ['points'],
  text: ['x', 'y', 'font-size', 'font-family', 'font-weight', 'text-anchor'],
  animateTransform: ['attributeName', 'type', 'from', 'to', 'dur', 'repeatCount'],
};
const PAINT = /^(none|#[0-9a-fA-F]{3}|#[0-9a-fA-F]{6}|black|white|red|green|blue|yellow|orange|gray|grey)$/;
const TRANSFORM = /^(\s*(translate|rotate|scale|matrix)\(\s*-?[\d.]+([\s,]+-?[\d.]+)*\s*\)\s*)+$/;
const ENTITIES = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'" };
const decode = s => s.replace(/&(\w+);/g, (m, e) => (e in ENTITIES ? ENTITIES[e] : fail('Сущность &' + e + ';')));

/**
 * Parses a symbol of the subset into {tag, attrs, children, text}. Its own small XML reader: no
 * DOCTYPE, no entities beyond the five basic ones, no processing instructions but the XML header.
 */
export function parseSvg(text) {
  if (typeof text !== 'string' || text.length > 64 * 1024) fail('SVG больше 64 КБ');
  let i = 0, count = 0;
  const s = text.replace(/^﻿/, '');
  const ws = () => { while (i < s.length && /\s/.test(s[i])) i++; };
  const skipMisc = () => {
    for (;;) {
      ws();
      if (s.startsWith('<?xml', i) && i === s.indexOf('<?xml')) { const e = s.indexOf('?>', i); if (e < 0) fail('XML'); i = e + 2; }
      else if (s.startsWith('<!--', i)) { const e = s.indexOf('-->', i); if (e < 0) fail('XML'); i = e + 3; }
      else if (s.startsWith('<!', i) || s.startsWith('<?', i)) fail('DOCTYPE и инструкции не допускаются');
      else return;
    }
  };
  function element(depth) {
    if (depth > 16 || ++count > 2000) fail('SVG слишком глубок или велик');
    if (s[i] !== '<') fail('Ожидался элемент');
    i++;
    const m = /^[A-Za-z][\w:-]*/.exec(s.slice(i, i + 80));
    if (!m) fail('Имя элемента');
    const tag = m[0].replace(/^svg:/, ''); i += m[0].length;
    const allowed = SVG_ELEMENTS[tag];
    if (!allowed) fail('<' + tag + '> вне набора');
    const attrs = {};
    for (;;) {
      ws();
      if (s.startsWith('/>', i)) { i += 2; return check({ tag, attrs, children: [], text: '' }); }
      if (s[i] === '>') { i++; break; }
      const a = /^([A-Za-z_][\w:.-]*)\s*=\s*("([^"<]*)"|'([^'<]*)')/.exec(s.slice(i, i + 70000));
      if (!a) fail('Атрибут у <' + tag + '>');
      i += a[0].length;
      const key = a[1];
      if (key === 'xmlns' || key.startsWith('xmlns:')) continue;
      if (!allowed.includes(key) && !COMMON.includes(key)) fail('<' + tag + ' ' + key + '> вне набора');
      attrs[key] = decode(a[3] ?? a[4]);
    }
    const children = []; let txt = '';
    for (;;) {
      if (i >= s.length) fail('Незакрытый <' + tag + '>');
      if (s.startsWith('</', i)) {
        const close = /^<\/([\w:-]+)\s*>/.exec(s.slice(i, i + 100));
        if (!close || close[1].replace(/^svg:/, '') !== tag) fail('Закрывающий тег');
        i += close[0].length;
        return check({ tag, attrs, children, text: txt.trim() });
      }
      if (s.startsWith('<!--', i)) { const e = s.indexOf('-->', i); if (e < 0) fail('XML'); i = e + 3; continue; }
      if (s.startsWith('<!', i) || s.startsWith('<?', i)) fail('CDATA и инструкции не допускаются');
      if (s[i] === '<') { children.push(element(depth + 1)); continue; }
      const e = s.indexOf('<', i); txt += decode(s.slice(i, e < 0 ? s.length : e)); i = e < 0 ? s.length : e;
    }
  }
  function check(n) {
    for (const k of ['fill', 'stroke']) if (n.attrs[k] !== undefined && !PAINT.test(n.attrs[k])) fail('Цвет ' + n.attrs[k]);
    if (n.attrs.transform !== undefined && !TRANSFORM.test(n.attrs.transform)) fail('Преобразование ' + n.attrs.transform);
    if (n.tag === 'animateTransform' && !(n.attrs.attributeName === 'transform' && n.attrs.type === 'rotate')) fail('Допускается только вращение');
    return n;
  }
  skipMisc();
  const root = element(0);
  skipMisc();
  if (i !== s.length) fail('Лишнее после </svg>');
  if (root.tag !== 'svg') fail('Корень не svg');
  const box = (root.attrs.viewBox ?? '').trim().split(/[\s,]+/).map(Number);
  if (box.length !== 4 || box.some(x => !Number.isFinite(x)) || box[2] <= 0 || box[3] <= 0) fail('Нужен viewBox');
  const aspect = root.attrs.preserveAspectRatio ?? '';
  if (!['', 'none', 'xMidYMid', 'xMidYMid meet'].includes(aspect)) fail('preserveAspectRatio ' + aspect);
  return { viewBox: box, stretch: aspect === 'none', root };
}

/** React's names for the subset's attributes. */
export const REACT_ATTRS = {
  'stroke-width': 'strokeWidth', 'fill-opacity': 'fillOpacity', 'stroke-opacity': 'strokeOpacity', 'stroke-linecap': 'strokeLinecap',
  'stroke-linejoin': 'strokeLinejoin', 'stroke-dasharray': 'strokeDasharray', 'font-size': 'fontSize', 'font-family': 'fontFamily',
  'font-weight': 'fontWeight', 'text-anchor': 'textAnchor',
};

/** A value for people: a translated range name, a number with its unit, on/off. */
export function format(port, v, decimals, unit, tr) {
  if (v === undefined || v === null || v === '' || v === 'null') return '—';
  const range = port?.range ?? [];
  const d = number(v);
  if (range.length && Number.isInteger(d) && d >= 0 && d < range.length) return tr(range[d]);
  if (port?.type?.endsWith('bool')) return v === 'true' ? 'вкл' : 'выкл';
  if (Number.isNaN(d) || v === 'true' || v === 'false') return String(v);
  const t = d.toFixed(decimals);
  return unit ? t + ' ' + unit : t;
}

/** Reads a kit's files (from /api/kitui) into {template, symbols, texts} for one widget id. */
export function loadWidget(kit, id, lang) {
  const files = kit?.files ?? {};
  const text = files[id + '.widget.json'];
  if (!text) fail('Нет виджета ' + id);
  const template = parseTemplate(text);
  if (template.id !== id) fail('id виджета не совпадает с именем файла');
  const symbols = {}, broken = [];
  for (const states of Object.values(template.symbols)) for (const f of Object.values(states)) {
    if (symbols[f] || broken.includes(f)) continue;
    try { symbols[f] = parseSvg(files[f] ?? fail('нет файла ' + f)); } catch (e) { broken.push(f + ': ' + e.message); }
  }
  let texts = {};
  try { const t = files['texts.' + lang + '.json']; if (t) texts = obj(JSON.parse(t)); } catch { texts = {}; }
  const tr = s => (typeof texts[s] === 'string' ? texts[s] : s);
  return { template, symbols, broken, tr };
}
