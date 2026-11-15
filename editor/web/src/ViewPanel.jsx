import React,{useState,useEffect,useRef}from'react';
import{newUi,validateUi,safeSvg}from'./uiDocument';
import{navigationIndex}from'./navigation';
import{bindingChoices,writablePort}from'./viewBindings';
export default function ViewPanel({graph,scope,consumer,writesEnabled,api,onDocument}){
 const[doc,setDoc]=useState(()=>graph.ui||newUi()),[list,setList]=useState([]),[chosen,setChosen]=useState(''),[message,setMessage]=useState(''),[busy,setBusy]=useState(false);const svgInput=useRef(),jsonInput=useRef();
 useEffect(()=>{setDoc(graph.ui||newUi())},[graph.ui]);
 const index=navigationIndex(graph),folder=index.path(scope).map(n=>n.name).join('/'),view=doc.views.find(v=>v.folder===folder);
 const[kind,setKind]=useState('value'),[component,setComponent]=useState(''),[slot,setSlot]=useState(''),[label,setLabel]=useState(''),[unit,setUnit]=useState(''),[decimals,setDecimals]=useState(1);
 const choices=bindingChoices(graph,folder),target=choices.find(c=>c.relative===component),ports=(target?.node.ports||[]).filter(p=>p.name!=='meta'&&!/password|secret|cred|^key$/i.test(p.name)&&(kind!=='setpoint'||writablePort(p))&&(kind!=='indicator'||p.type.replace('sys::','')==='bool'));
 useEffect(()=>{setComponent('');setSlot('')},[folder]);
 function updateView(next){adopt({...doc,views:[...doc.views.filter(v=>v.folder!==folder),next]})}
 function addWidget(){action(async()=>{const p=ports.find(p=>p.name===slot);if(!view||!target||!p)throw Error('Выберите компонент и слот');const widgets=view.widgets||[];if(widgets.length>=64)throw Error('Не более 64 виджетов');updateView({...view,widgets:[...widgets,{id:crypto.randomUUID(),kind,label:label||target.node.name+'.'+p.name,path:target.relative,slot:p.name,type:p.type,x:5+(widgets.length%4)*22,y:8+Math.floor(widgets.length/4)%5*16,unit,decimals:+decimals}]});setMessage('Виджет добавлен. Откройте экран и разместите его за заголовок.');})}
 const[url,setUrl]=useState('');useEffect(()=>{if(!view?.svg){setUrl('');return;}try{const u=URL.createObjectURL(new Blob([safeSvg(view.svg)],{type:'image/svg+xml'}));setUrl(u);return()=>URL.revokeObjectURL(u)}catch(e){setUrl('');setMessage(e.message)}},[view?.svg]);
 function adopt(d){const v=validateUi(d);for(const item of v.views)if(item.svg)item.svg=safeSvg(item.svg);for(const asset of v.library||[])for(const key of Object.keys(asset.states))asset.states[key]=safeSvg(asset.states[key]);setDoc(v);onDocument(v);}
 async function action(fn){setBusy(true);try{await fn()}catch(e){setMessage(e.message)}finally{setBusy(false)}}
 async function refresh(){setList(await api('ui/list'));}
 useEffect(()=>{refresh().catch(e=>setMessage(e.message))},[]);
 async function importSvg(file){if(!file)return;await action(async()=>{if(file.size>512*1024)throw Error('SVG больше 512 KiB');const svg=safeSvg(await file.text()),next={...view,id:view?.id||crypto.randomUUID(),title:file.name.replace(/\.svg$/i,''),folder,svg};updateView(next);setMessage('SVG прикреплён к папке. Сохраните пакет локально или на устройство.');});svgInput.current.value='';}
 function download(){const u=URL.createObjectURL(new Blob([JSON.stringify(doc,null,2)],{type:'application/json'})),a=document.createElement('a');a.href=u;a.download='mcs-ui.json';a.click();setTimeout(()=>URL.revokeObjectURL(u),1000);}
 return <div className="panel"><div className="eyebrow">ВИД ПАПКИ</div><h3>Визуализация</h3><p>Папка: <b>{folder||'Корень проекта'}</b></p><p className="muted">Выберите папку в «Обзор», затем прикрепите SVG. Пакет хранится отдельно от app.sab.</p>
 <label>Название пакета<input aria-label="Название визуализации" value={doc.title} onChange={e=>adopt({...doc,title:e.target.value})}/></label>
 <button disabled={busy} onClick={()=>svgInput.current.click()}>Прикрепить SVG к папке</button>
 {!view&&<button onClick={()=>updateView({id:crypto.randomUUID(),title:'Экран '+(folder||'проекта'),folder,svg:'',widgets:[]})}>Создать пустой экран</button>}
 {view&&<><h4>{view.title}</h4>{url&&<img className="ui-svg-preview" alt={view.title} src={url}/>}<button onClick={()=>window.dispatchEvent(new CustomEvent('mcs-show-view',{detail:{view}}))}>Открыть экран</button><p className="muted">На экране доступна библиотека отдельных SVG: перетаскивание, привязки и состояния.</p>
 <div className="widget-builder"><h4>Добавить виджет</h4><label>Виджет<select aria-label="Вид виджета" value={kind} onChange={e=>{setKind(e.target.value);setSlot('')}}><option value="value">Значение</option><option value="indicator">Индикатор bool</option><option value="setpoint">Уставка</option></select></label>
 <label>Компонент<select aria-label="Компонент виджета" value={component} onChange={e=>{setComponent(e.target.value);setSlot('')}}><option value="">Выберите компонент</option>{choices.map(c=><option key={c.node.id} value={c.relative}>{c.relative} · {c.node.type}</option>)}</select></label>
 <label>Слот<select aria-label="Слот виджета" value={slot} onChange={e=>setSlot(e.target.value)}><option value="">Выберите слот</option>{ports.map(p=><option key={p.id} value={p.name}>{p.name} · {p.type}</option>)}</select></label>
 <label>Подпись<input aria-label="Подпись виджета" value={label} maxLength={128} onChange={e=>setLabel(e.target.value)}/></label><label>Единица<input aria-label="Единица виджета" value={unit} maxLength={32} onChange={e=>setUnit(e.target.value)}/></label><label>Знаков после запятой<input aria-label="Точность виджета" type="number" min="0" max="6" value={decimals} onChange={e=>setDecimals(e.target.value)}/></label><button disabled={busy||!slot||!target} onClick={addWidget}>Добавить виджет</button>
 {(view.widgets||[]).map(w=><div className="widget-list-row" key={w.id}><span>{w.label}<small>{w.path}/{w.slot}</small></span><button aria-label={'Удалить виджет '+w.label} onClick={()=>updateView({...view,widgets:view.widgets.filter(x=>x.id!==w.id)})}>×</button></div>)}</div></>}
 <button disabled={busy} onClick={()=>action(async()=>{const r=await api('ui/save',{document:doc});await refresh();setMessage('Сохранено на ПК: '+r.path)})}>Сохранить на ПК</button>
 <label>Сохранённые пакеты<select aria-label="Сохранённые визуализации" value={chosen} onChange={e=>setChosen(e.target.value)}><option value="">Выберите пакет</option>{list.map(p=><option key={p.id} value={p.id}>{p.title}</option>)}</select></label>
 <button disabled={busy||!chosen} onClick={()=>action(async()=>{adopt(await api('ui/load',{id:chosen}));setMessage('Пакет загружен с ПК')})}>Загрузить с ПК</button>
 <button disabled={busy||!consumer||!writesEnabled} onClick={()=>action(async()=>{const r=await api('ui/device/save',{consumer,document:doc});setMessage('Записано на устройство: '+r.uri+' · обратное чтение совпало');await refresh()})}>Сохранить на устройство</button>
 <button disabled={busy||!consumer} onClick={()=>action(async()=>{adopt(await api('ui/device/load',{consumer}));setMessage('Пакет получен с устройства и сохранён на ПК');await refresh()})}>Прочитать с устройства</button>
 <button onClick={download}>Экспортировать пакет</button><button onClick={()=>jsonInput.current.click()}>Открыть пакет</button>
 <input style={{display:'none'}} ref={svgInput} data-testid="ui-svg-file" type="file" accept=".svg,image/svg+xml" onChange={e=>importSvg(e.target.files[0])}/><input style={{display:'none'}} ref={jsonInput} data-testid="ui-json-file" type="file" accept=".json" onChange={e=>{const f=e.target.files[0];action(async()=>{if(!f||f.size>512*1024)throw Error('Пакет больше 512 KiB');adopt(JSON.parse(await f.text()));setMessage('Пакет открыт');});e.target.value=''}}/>
 <p className="note">{doc.views.length} экранов · до 512 KiB. На устройстве: mcs-ui.json относительно каталога, доступного SOX. Запись файла не атомарна; локальная копия сохраняется до отправки.</p>{message&&<p role="status">{message}</p>}
 {busy&&<p role="status">Операция с пакетом…</p>}</div>;
}
