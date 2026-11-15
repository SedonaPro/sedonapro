import test from 'node:test'; import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs'; import { join, dirname } from 'node:path'; import { fileURLToPath } from 'node:url';
import { parseTemplate, parseSvg, parseCondition, testCondition, problems, partsOf, loadWidget, format } from '../web/src/kitWidget.js';

// The Ahu widget as a kit ships it, and its type as the device describes it (from the kit manifest).
const root = join(dirname(fileURLToPath(import.meta.url)), '../..');
const kitDir = join(root, 'kits/hvac/sproAhu/ui');
const files = {};
for (const f of ['ahu.widget.json', 'texts.ru.json']) files[f] = readFileSync(join(kitDir, f), 'utf8');
for (const f of readdirSync(join(kitDir, 'svg'))) files['svg/' + f] = readFileSync(join(kitDir, 'svg', f), 'utf8');
const manifestDir = join(root, 'build/home/manifests/sproAhu');
const manifest = readdirSync(manifestDir).map(f => readFileSync(join(manifestDir, f), 'utf8')).find(x => x.includes('name="widget"'));
function ahuNode() {
  const type = manifest.slice(manifest.indexOf('name="Ahu"'));
  const ports = [], actions = [];
  for (const m of type.matchAll(/<slot id="(\d+)" name="(\w+)" type="([\w:]+)"( flags="(\w*)")?[^>]*?(\/>|>([\s\S]*?)<\/slot>)/g)) {
    const flags = m[5] ?? '', facets = m[7] ?? '';
    if (flags.includes('a')) { if (m[3] === 'void') actions.push(m[2]); continue; }
    const range = /name="range" val="([^"]*)"/.exec(facets);
    ports.push({ id: +m[1], name: m[2], type: m[3], config: flags.includes('c'), readonly: /name="readonly"/.test(facets), ...(range ? { range: range[1].split(',').map(s => s.trim()) } : {}) });
  }
  return { id: 'n5', name: 'unit', type: 'sproAhu::Ahu', parent: 0, widget: 'ahu', ports, actions };
}

test('the Ahu widget parses, its symbols are in the subset, it matches the device type', () => {
  const w = loadWidget({ files }, 'ahu', 'ru');
  assert.equal(w.broken.length, 0, w.broken.join('; '));
  assert.ok(Object.keys(w.symbols).length >= 20);
  assert.equal(w.tr('Supply fan'), 'Приточный вентилятор');
  assert.equal(w.tr('unknown text'), 'unknown text');
  const comp = ahuNode();
  assert.ok(comp.actions.includes('build'), 'build action from the manifest');
  assert.deepEqual(problems(w.template, comp, {}), []);
});

test('mismatches are reported', () => {
  const comp = ahuNode(), text = files['ahu.widget.json'];
  assert.ok(problems(parseTemplate(text.replace('"slot": "layout"', '"slot": "parts"')), comp, {}).some(p => p.includes('parts')));
  assert.ok(problems(parseTemplate(text.replace('"invoke": "build"', '"invoke": "reset"')), comp, {}).some(p => p.includes('reset')));
  assert.ok(problems(parseTemplate(text.replace("'supplyExhaust'", "'both'")), comp, {}).some(p => p.includes("'both'")));
  assert.ok(problems(parseTemplate(text), { ...comp, widget: undefined }, {}).some(p => p.includes('не указывает')));
  const parts = { fanS: { id: 'n9', name: 'fanS', type: 'sproHvac::FanSpeed', ports: [{ name: 'other', type: 'float' }] } };
  assert.ok(problems(parseTemplate(text), comp, parts).some(p => p.includes('fanS.out')));
  const heater = { heater: { id: 'n10', name: 'heater', type: 'sproHvac::WaterHeater', ports: [{ name: 'valve', type: 'float' }, { name: 'frost', type: 'bool' }, { name: 'state', type: 'byte' }] } };
  assert.deepEqual(problems(parseTemplate(text), comp, heater), [], 'a slot of the other heater variant is not a mismatch');
});

test('conditions', () => {
  const ports = Object.fromEntries(ahuNode().ports.map(p => [p.name, p]));
  const c = parseCondition("layout == 'supplyExhaust' && recovery != 'none' || heater == 'electric'");
  assert.equal(testCondition(c, ports, { layout: '1', recovery: '2', heater: '1' }), true);
  assert.equal(testCondition(c, ports, { layout: '1', recovery: '0', heater: '1' }), false);
  assert.equal(testCondition(c, ports, { layout: '0', recovery: '0', heater: '2' }), true);
  assert.equal(testCondition(parseCondition(''), ports, {}), true);
  assert.throws(() => parseCondition('x() == 1'));
  assert.throws(() => parseCondition('a == b'));
});

test('parts are children of the component with an allowed type', () => {
  const t = parseTemplate(files['ahu.widget.json']);
  const graph = { nodes: [{ id: 'n5', name: 'unit', parent: 0 }, { id: 'n6', name: 'fanS', type: 'sproHvac::FanSpeed', parent: 5 }, { id: 'n7', name: 'tSup', type: 'sproHvac::FanSpeed', parent: 5 }, { id: 'n8', name: 'fanE', type: 'sproHvac::FanSpeed', parent: 2 }] };
  assert.deepEqual(Object.keys(partsOf(t, graph, 'n5')), ['fanS']);
});

test('hostile templates and symbols are refused', () => {
  const text = files['ahu.widget.json'];
  assert.throws(() => parseTemplate(text.replace('sedonapro-widget/1', 'x/2')));
  assert.throws(() => parseTemplate(text.replace('svg/fan-on.svg', '../fan-on.svg')));
  assert.throws(() => parseTemplate(text.replace('"fanS.out"', '"other.out"')));
  assert.throws(() => parseTemplate(text.replace('"x": 610', '"x": 1e9')));
  const head = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10">';
  for (const bad of [head + '<script>x()</script></svg>', head + '<rect onclick="x()"/></svg>', head + '<rect style="fill:red"/></svg>',
    head + '<image href="#a"/></svg>', head + '<rect fill="url(#g)"/></svg>', head + '<foreignObject/></svg>',
    '<?xml version="1.0"?><!DOCTYPE svg [<!ENTITY x SYSTEM "file:///etc/passwd">]>' + head + '<text>&x;</text></svg>',
    head + '<text>&x;</text></svg>', head + '<![CDATA[x]]></svg>', '<svg xmlns="http://www.w3.org/2000/svg"><rect/></svg>',
    head + '<rect transform="skewX(10)"/></svg>', head + '<rect/></svg><script/>', head + '<a href="http://x"><rect/></a></svg>'])
    assert.throws(() => parseSvg(bad), undefined, bad);
  const ok = parseSvg('<?xml version="1.0"?>' + head + '<!-- c --><g transform="rotate(45 5 5)"><rect x="1" y="1" width="2" height="2" fill="#abc"/><text x="1" y="5">a &amp; b</text></g></svg>');
  assert.equal(ok.root.children[0].children[1].text, 'a & b');
});

test('values for people', () => {
  const tr = s => ({ winter: 'зима' })[s] ?? s;
  assert.equal(format({ type: 'byte', range: ['summer', 'winter'] }, '1', 0, '', tr), 'зима');
  assert.equal(format({ type: 'float' }, '21.456', 1, '°C', tr), '21.5 °C');
  assert.equal(format({ type: 'bool' }, 'true', 0, '', tr), 'вкл');
  assert.equal(format({ type: 'float' }, undefined, 1, '', tr), '—');
});
