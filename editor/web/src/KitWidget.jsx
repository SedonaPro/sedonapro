import React, { useEffect, useMemo, useState } from 'react';
import { values } from './model';
import { loadWidget, partsOf, problems, target, parseCondition, testCondition, number, format, REACT_ATTRS } from './kitWidget';

// A kit widget (docs/widgets.md) over one device component: "Настройка" - options on the picture
// and the widget's actions (confirmed, with the options they write); "Работа" - live values and
// controls written one by one with the usual prepare/confirm/readback.

function svgNode(n, key) {
  const props = { key };
  for (const [k, v] of Object.entries(n.attrs)) if (k !== 'id') props[REACT_ATTRS[k] ?? k] = v;
  const kids = n.children.map((c, i) => svgNode(c, i));
  if (n.text) kids.push(n.text);
  return React.createElement(n.tag, props, ...kids);
}

function Symbol({ symbol, e }) {
  const root = symbol.root, inherit = {};
  for (const [k, v] of Object.entries(root.attrs)) if (!['xmlns', 'viewBox', 'width', 'height', 'version', 'preserveAspectRatio'].includes(k)) inherit[REACT_ATTRS[k] ?? k] = v;
  return <svg x={e.x} y={e.y} width={e.w} height={e.h} viewBox={symbol.viewBox.join(' ')} preserveAspectRatio={symbol.stretch ? 'none' : 'xMidYMid meet'} overflow="hidden">
    <g {...inherit}>{root.children.map((c, i) => svgNode(c, i))}</g>
  </svg>;
}

const truthy = v => { const d = number(v); return !Number.isNaN(d) && d > 0; };

export default function KitWidget({ nodeId, graph, consumer, status, fresh, api, onGraph, onClose }) {
  const comp = graph.nodes.find(n => n.id === nodeId);
  const [loaded, setLoaded] = useState(null), [error, setError] = useState(''), [face, setFace] = useState(null);
  const [draft, setDraft] = useState({}), [focus, setFocus] = useState([]), [plan, setPlan] = useState(null), [write, setWrite] = useState(null);
  const [message, setMessage] = useState(''), [busy, setBusy] = useState(false), [, setTick] = useState(0);
  const kitName = comp?.kitPart?.replace(/-[0-9a-f]+$/, '');
  useEffect(() => {
    let gone = false;
    (async () => {
      try {
        const r = await api('kitui', { consumer: consumer || '' });
        const kit = r.kits?.[kitName];
        if (!kit) throw new Error('Виджета kit ' + kitName + ' нет ни на устройстве, ни в SDK. ' + (r.deviceProblem || ''));
        const w = loadWidget(kit, comp.widget, 'ru');
        if (!gone) setLoaded({ ...w, source: r.source === 'device' ? 'Виджет с устройства (m:ui.zip)' : 'Виджет из SDK: ' + r.path });
      } catch (e) { if (!gone) setError(e.message); }
    })();
    return () => { gone = true; };
  }, [nodeId, kitName]);
  useEffect(() => { const t = setInterval(() => setTick(x => x + 1), 1000); return () => clearInterval(t); }, []);
  const parts = useMemo(() => (loaded && comp ? partsOf(loaded.template, graph, nodeId) : {}), [loaded, graph, nodeId]);
  const mismatch = useMemo(() => (loaded && comp ? problems(loaded.template, comp, parts) : []), [loaded, comp, parts]);
  if (!comp) return <div className="kit-widget" role="dialog"><p role="alert">Компонента больше нет на устройстве.</p><button onClick={onClose}>Закрыть</button></div>;
  if (error || mismatch.length) return <div className="kit-widget" role="dialog" aria-label="Виджет"><div className="kit-widget-head"><b>{comp.name}</b><button onClick={onClose}>Закрыть</button></div><p role="alert">{error || 'Виджет не совпадает с компонентом устройства и не показан: ' + mismatch.slice(0, 4).join('; ')}</p></div>;
  if (!loaded) return <div className="kit-widget" role="dialog" aria-label="Виджет"><p>Загрузка виджета…</p></div>;

  const { template: t, symbols, tr } = loaded;
  const own = Object.fromEntries(comp.ports.map(p => [p.name, p]));
  const device = values.get(nodeId);
  const setupFace = (face ?? (Object.keys(parts).length ? 'operation' : 'setup')) === 'setup';
  const effective = { ...device, ...(setupFace ? draft : {}) };
  const when = w => testCondition(parseCondition(w), own, effective);
  const read = ref => { const x = target(t, comp, parts, ref); return x ? { ...x, value: values.get(x.node.id)[x.port.name] } : null; };
  const live = !!consumer && status.mode === 'LIVE' && fresh && status.writesEnabled && !status.fileTransfer;
  const changed = Object.entries(draft).filter(([k, v]) => String(device[k]) !== String(v));

  async function prepareAction(a) {
    setBusy(true); setMessage('');
    try { setPlan({ ...(await api('action/prepare', { consumer, componentId: +nodeId.slice(1), action: a.invoke, values: Object.fromEntries(changed) })), widgetAction: a }); }
    catch (e) { setMessage(e.message); } finally { setBusy(false); }
  }
  async function commitAction() {
    const p = plan; setPlan(null); setBusy(true);
    try { const r = await api('action/commit', { consumer, token: p.token }); setDraft({}); onGraph(r.graph); setMessage(tr(p.widgetAction.label) + ': выполнено, устройство перечитано. App.save не вызывался.'); }
    catch (e) { setMessage(e.message + ' Автоповтора нет.'); } finally { setBusy(false); }
  }
  function cancelAction() { if (plan) api('action/cancel', { consumer, token: plan.token }).catch(() => {}); setPlan(null); }
  async function prepareWrite(ref, value, label) {
    const x = read(ref); if (!x) return;
    setBusy(true); setMessage('');
    try { setWrite({ label, ...(await api('write/prepare', { consumer, componentId: +x.node.id.slice(1), slotId: x.port.id, value: String(value) })) }); }
    catch (e) { setMessage(e.message); } finally { setBusy(false); }
  }
  async function commitWrite() {
    const w = write; setWrite(null); setBusy(true);
    try { const r = await api('write/commit', { consumer, token: w.token }); setMessage(r.status === 'VERIFIED' ? w.label + ': подтверждено чтением ' + r.readBack : 'Обратное чтение не совпало: ' + r.readBack); }
    catch (e) { setMessage(e.message + ' Автоповтора нет.'); } finally { setBusy(false); }
  }
  function cancelWrite() { if (write) api('write/cancel', { consumer, token: write.token }).catch(() => {}); setWrite(null); }

  const elements = [], labels = [];
  for (const e of t.elements) {
    const shown = when(e.when), ghost = !shown && e.ghost && setupFace;
    if (!shown && !ghost) continue;
    let state = 'off';
    if (shown && !setupFace && e.fault && truthy(read(e.fault)?.value)) state = 'fault';
    else if (shown && !setupFace && e.state && truthy(read(e.state)?.value)) state = 'on';
    const files = t.symbols[e.symbol] ?? {}, symbol = symbols[files[state] ?? files.off];
    const chosen = setupFace && focus.length && focus.join() === e.configure.join();
    elements.push(<g key={e.id} data-element={e.id} opacity={ghost ? 0.28 : 1} style={setupFace && e.configure.length ? { cursor: 'pointer' } : undefined} onClick={setupFace && e.configure.length ? () => setFocus(e.configure) : undefined}>
      <title>{ghost ? tr(e.label) + ' — нет в установке; щёлкните, чтобы выбрать' : tr(e.label)}</title>
      {symbol ? <Symbol symbol={symbol} e={e} /> : <rect x={e.x} y={e.y} width={e.w} height={e.h} fill="#eef1f3" stroke="#c3cbd1" />}
      {(ghost || chosen) && <rect x={e.x - 3} y={e.y - 3} width={e.w + 6} height={e.h + 6} fill="none" stroke={ghost ? '#718391' : '#2f80c4'} strokeDasharray="6 4" />}
    </g>);
    if (!shown || setupFace) continue;
    for (const v of e.values) {
      if (!when(v.when)) continue;
      const x = read(v.bind), text = (v.label ? tr(v.label) + ' ' : '') + format(x?.port, x?.value, v.decimals, v.unit, tr);
      labels.push(<text key={e.id + v.bind} x={e.x + e.w / 2 + v.dx} y={e.y + v.dy + 13} textAnchor="middle" fontSize="13" fill="#1f2a33">{text}</text>);
    }
  }
  const pct = (v, total) => (v / total * 100) + '%';

  return <div className="kit-widget" role="dialog" aria-label={'Виджет ' + comp.name}>
    <div className="kit-widget-head">
      <b>{tr(t.title)} · {comp.name}</b>
      <div className="kit-widget-faces" role="tablist">
        <button role="tab" aria-selected={setupFace} className={setupFace ? 'chosen' : ''} onClick={() => setFace('setup')}>Настройка</button>
        <button role="tab" aria-selected={!setupFace} className={!setupFace ? 'chosen' : ''} onClick={() => setFace('operation')}>Работа</button>
      </div>
      <small>{loaded.source}{loaded.broken.length ? ' · значки с ошибкой: ' + loaded.broken.length : ''}</small>
      <button onClick={onClose}>Закрыть</button>
    </div>
    <div className="kit-widget-body">
      <div className="kit-widget-stage" style={{ aspectRatio: t.width + ' / ' + t.height, '--ratio': t.width / t.height }}>
        <svg viewBox={`0 0 ${t.width} ${t.height}`} width="100%" height="100%" role="img" aria-label={tr(t.title)} onClick={e => { if (e.target === e.currentTarget) setFocus([]); }}>
          <rect width={t.width} height={t.height} fill="#fbfcfd" stroke="#d7dde2" />
          {t.texts.filter(x => when(x.when)).map((x, i) => <text key={i} x={x.x} y={x.y + 14} fontSize="14" fill="#4f5b63">{tr(x.text)}</text>)}
          {elements}{labels}
        </svg>
        {!setupFace && t.controls.filter(c => when(c.when)).map(c => {
          const x = read(c.bind); if (!x) return null;
          const label = tr(c.label), style = { left: pct(c.x, t.width), top: pct(c.y, t.height) };
          let input;
          if (c.kind === 'status') input = <b>{format(x.port, x.value, 1, c.unit, tr)}</b>;
          else if (c.kind === 'mode') input = <select aria-label={label} value={String(number(x.value))} disabled={!live || busy} onChange={ev => prepareWrite(c.bind, ev.target.value, label)}>{(x.port.range ?? []).map((r, i) => <option key={r} value={String(i)}>{tr(r)}</option>)}</select>;
          else if (c.kind === 'toggle') input = <input type="checkbox" aria-label={label} checked={truthy(x.value)} disabled={!live || busy} onChange={ev => prepareWrite(c.bind, ev.target.checked, label)} />;
          else input = <SetpointInput label={label} value={x.value} unit={c.unit} disabled={!live || busy} onSet={v => prepareWrite(c.bind, v, label)} />;
          return <div key={c.bind} className="kit-widget-control" style={style}><small>{label}</small>{input}</div>;
        })}
      </div>
      {setupFace && <aside className="kit-widget-side" aria-label="Опции">
        <b>Опции</b>
        {t.options.filter(o => when(o.when) && own[o.slot]).map(o => {
          const p = own[o.slot], v = effective[o.slot], label = tr(o.label), mark = changed.some(([k]) => k === o.slot);
          const set = val => setDraft(d => ({ ...d, [o.slot]: String(val) }));
          let input;
          if (p.range?.length) input = <select aria-label={label} value={String(number(v))} onChange={e => set(e.target.value)}>{p.range.map((r, i) => <option key={r} value={String(i)}>{tr(r)}</option>)}</select>;
          else if (p.type.endsWith('bool')) input = <input type="checkbox" aria-label={label} checked={v === 'true'} onChange={e => set(e.target.checked)} />;
          else input = <input aria-label={label} value={v ?? ''} onChange={e => set(e.target.value)} />;
          return <label key={o.slot} className={'kit-widget-option' + (focus.includes(o.slot) ? ' focused' : '')}><span>{label}{mark ? ' ●' : ''}</span>{input}</label>;
        })}
        {t.info.length > 0 && <b>Сведения</b>}
        {t.info.map(i => { const x = read(i.bind); const asText = x && /Buf|Str/.test(x.port.type) || (x?.value ?? '').length > 40; return <div key={i.bind} className="kit-widget-info"><small>{tr(i.label)}</small>{asText ? <pre>{String(x?.value ?? '').replace(/\\n/g, '\n')}</pre> : <span>{format(x?.port, x?.value, 0, '', tr)}</span>}</div>; })}
        {t.actions.map(a => <button key={a.id} disabled={!live || busy} title={live ? '' : 'Нужно подключение LIVE с разрешённой записью'} onClick={() => prepareAction(a)}>{tr(a.label)}…</button>)}
        {changed.length > 0 && <small className="kit-widget-pending">Ещё не на устройстве: {changed.map(([k]) => tr(t.options.find(o => o.slot === k)?.label ?? k)).join(', ')}. Они запишутся действием.</small>}
      </aside>}
    </div>
    {plan && <div className="kit-widget-confirm" role="alertdialog" aria-label="Подтверждение действия">
      <b>{tr(plan.widgetAction.label)} · {plan.component}</b><p>{tr(plan.widgetAction.confirm)}</p>
      {plan.changes.length > 0 && <><p>Сначала запишутся опции (с проверкой обратным чтением):</p><ul>{plan.changes.map(c => <li key={c.slot}>{tr(t.options.find(o => o.slot === c.slot)?.label ?? c.slot)}: {format(own[c.slot], c.before, 0, '', tr)} → {format(own[c.slot], c.desired, 0, '', tr)}</li>)}</ul></>}
      <button onClick={commitAction}>Подтвердить</button><button onClick={cancelAction}>Отмена</button>
    </div>}
    {write && <div className="kit-widget-confirm" role="alertdialog" aria-label="Подтверждение записи"><b>{write.label}</b><p>Было: {write.before} → {write.desired}</p><button onClick={commitWrite}>Подтвердить</button><button onClick={cancelWrite}>Отмена</button></div>}
    {message && <p role="status" className="kit-widget-message">{message}</p>}
    {!live && <small className="kit-widget-message">Управление и действия — только при подключении LIVE с разрешённой записью; опции можно выбирать заранее.</small>}
  </div>;
}

function SetpointInput({ label, value, unit, disabled, onSet }) {
  const [text, setText] = useState('');
  const d = number(value);
  return <span className="kit-widget-setpoint"><input aria-label={label} placeholder={Number.isNaN(d) ? '' : d.toFixed(1)} value={text} disabled={disabled} onChange={e => setText(e.target.value)} onKeyDown={e => { if (e.key === 'Enter' && text.trim()) onSet(text.trim()); }} /><button disabled={disabled || !text.trim()} onClick={() => onSet(text.trim())}>Задать</button>{unit}</span>;
}
