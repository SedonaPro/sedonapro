// Graphic components on a screen (UI format v3): the handles that move a placed part as a whole
// and the panel that adds parts from a library and feeds their points from one block.
import React,{useEffect,useState}from'react';
import{validateComponents,autoBind}from'./components';import{imageUri}from'./svgImage';import{validateLibrary}from'./svgObjects';import{safeSvg}from'./uiDocument';import{bindingChoices}from'./viewBindings';
export function PartHandles({view,components,canvas,selected,onSelect,onMove}){
 const[drag,setDrag]=useState(null);
 return (view.parts||[]).map(part=>{const def=components.find(c=>c.id===part.componentId);if(!def)return null;const at=drag?.id===part.id?drag:part;
  function down(e){e.preventDefault();e.stopPropagation();onSelect(part.id);const stage=e.currentTarget.parentElement.getBoundingClientRect();e.currentTarget.setPointerCapture(e.pointerId);setDrag({id:part.id,sx:e.clientX,sy:e.clientY,stage,x:part.x,y:part.y})}
  function move(e){if(drag?.id!==part.id)return;const limitX=100-def.width/canvas.width*100,limitY=100-def.height/canvas.height*100;setDrag({...drag,x:Math.max(0,Math.min(limitX,part.x+(e.clientX-drag.sx)/drag.stage.width*100)),y:Math.max(0,Math.min(limitY,part.y+(e.clientY-drag.sy)/drag.stage.height*100))})}
  function up(){if(drag?.id===part.id){if(Math.abs(drag.x-part.x)+Math.abs(drag.y-part.y)>.001)onMove(part.id,{x:drag.x,y:drag.y});setDrag(null)}}
  return <div key={part.id} data-part-id={part.id} className={'part-handle '+(selected===part.id?'part-selected':'')} title={part.label} style={{left:at.x+'%',top:at.y+'%',width:def.width/canvas.width*100+'%',height:def.height/canvas.height*100+'%'}} onClick={e=>e.stopPropagation()} onPointerDown={down} onPointerMove={move} onPointerUp={up} onPointerCancel={()=>setDrag(null)}><span className="svg-object-caption">{part.label}</span></div>});
}
let loaded=null;
// "group" ("Air/Fans") is the item's folder in the palette; it lives in the library only.
function readLibrary(lib){if(!lib||lib.format!=='mcs-sedona-ui-library'||lib.version!==1||!Array.isArray(lib.assets))throw Error('Это не библиотека компонентов');const groups=Object.fromEntries((lib.components||[]).filter(c=>typeof c.group==='string'&&c.group.length<=128).map(c=>[c.id,c.group]));return {title:String(lib.title||lib.id),assets:lib.assets,components:validateComponents(lib.components||[],lib.assets,256),groups};}
// Folders within folders: {folders:{name:node},items:[component]} built from the "A/B" groups.
export function groupTree(list,groupOf){const root={folders:{},items:[]};for(const c of list){let at=root;for(const name of String(groupOf(c)||'').split('/').filter(Boolean))at=at.folders[name]||(at.folders[name]={folders:{},items:[]});at.items.push(c);}return root;}
// What a component looks like at work, before it is placed: its pictures in the "on" drawing and
// a sample number where a value goes. Nothing is read from the device here.
function PartPreview({def,assets}){
 const scale=Math.min(1,250/def.width,150/def.height),fractions=new Set((def.objects||[]).filter(o=>o.mode==='position'&&o.binding).map(o=>o.binding.slot));
 return <div className="part-preview" aria-label={'Предпросмотр: '+def.title}><div style={{position:'relative',width:def.width*scale,height:def.height*scale,margin:'0 auto'}}>
  {(def.objects||[]).map(o=>{const a=assets.find(x=>x.id===o.assetId);if(!a)return null;const svg=o.mode==='position'?a.states.p50||a.states.off:o.mode==='bool'?a.states.on||a.states.off:a.states.off;return <img key={o.id} alt="" draggable={false} src={imageUri(svg,true)} style={{position:'absolute',left:o.x+'%',top:o.y+'%',width:a.width*scale,height:a.height*scale,transform:`rotate(${o.rotation}deg)`}}/>})}
  {(def.widgets||[]).filter(w=>w.kind!=='text').map(w=><span key={w.id} style={{position:'absolute',left:w.x+'%',top:w.y+'%',fontSize:Math.max(9,(w.fontSize||15)*scale),whiteSpace:'nowrap',color:'#1c2a25'}}>{fractions.has(w.slot)?'0.50':(42.5).toFixed(w.decimals)} <small>{w.unit}</small></span>)}</div>
  <p><b>{def.title}</b><br/>{def.points.map(p=>p.label).join(' · ')}</p></div>;
}
function Folder({name,node,chosen,onChoose,onAdd,depth=0}){
 return <details className="part-folder" open={depth<1}><summary>{name}</summary>
  {Object.entries(node.folders).map(([n,child])=><Folder key={n} name={n} node={child} chosen={chosen} onChoose={onChoose} onAdd={onAdd} depth={depth+1}/>)}
  {node.items.map(c=><div key={c.id} className={'svg-asset-card '+(chosen===c.id?'part-chosen':'')} onClick={()=>onChoose(c.id)}><span>{c.title}</span><button aria-label={'Добавить '+c.title} onClick={e=>{e.stopPropagation();onAdd(c)}}>+</button></div>)}</details>;
}
export function PartPanel({view,graph,components,documentAssets=[],part,onAdd,onChange,onRemove,onClose}){
 const[library,setLibrary]=useState(loaded),[message,setMessage]=useState(''),[label,setLabel]=useState(part?.label||''),[chosen,setChosen]=useState('');
 useEffect(()=>{if(loaded)return;let active=true;import('./hvacLibrary.json').then(m=>{loaded=readLibrary(m.default);if(active)setLibrary(loaded)}).catch(e=>setMessage(e.message));return()=>{active=false}},[]);
 useEffect(()=>setLabel(part?.label||''),[part?.id,part?.label]);
 async function open(file){if(!file)return;try{if(file.size>8*1024*1024)throw Error('Файл больше 8 MiB');loaded=readLibrary(JSON.parse(await file.text()));setLibrary(loaded);setMessage('')}catch(e){setMessage(e.message)}}
 function add(def){try{const own=components.some(c=>c.id===def.id),ids=new Set((def.objects||[]).map(o=>o.assetId)),assets=own?[]:validateLibrary((library?.assets||[]).filter(a=>ids.has(a.id))).map(a=>({...a,states:Object.fromEntries(Object.entries(a.states).map(([k,v])=>[k,safeSvg(v)]))}));onAdd(def,assets);setMessage('')}catch(e){setMessage(e.message)}}
 const list=[...(library?.components||[]),...components.filter(c=>!(library?.components||[]).some(x=>x.id===c.id))],def=part&&components.find(c=>c.id===part.componentId),choices=part?bindingChoices(graph,view.folder):[];
 const tree=groupTree(list,c=>library?.components.some(x=>x.id===c.id)?library.title+'/'+(library.groups[c.id]||''):'В проекте'),shown=list.find(c=>c.id===chosen);
 return <div className="svg-tools part-panel"><button className="svg-close-tools" onClick={onClose}>Скрыть</button><h3>Компоненты{library?' · '+library.title:''}</h3>
  {part&&def&&<div className="part-inspector"><h4>{def.title}</h4><label>Подпись<input aria-label="Подпись компонента" value={label} maxLength={128} onChange={e=>setLabel(e.target.value)} onBlur={()=>label!==part.label&&onChange(part.id,{label})}/></label>
   <label>Привязать к блоку<select aria-label="Блок компонента" value="" onChange={e=>{if(!e.target.value)return;const bind=autoBind(choices,def,e.target.value);onChange(part.id,{bind:{...part.bind,...bind},label:part.label===def.title?e.target.value.split('/').at(-1):part.label});setMessage('Точек привязано: '+Object.keys(bind).length+' из '+def.points.length)}}><option value="">Выберите блок</option>{choices.map(c=><option key={c.node.id} value={c.relative}>{c.relative}</option>)}</select></label>
   <ul className="part-points">{def.points.map(p=><li key={p.id}><b>{p.label}</b>: {part.bind[p.id]?part.bind[p.id].path+' · '+part.bind[p.id].slot:'не привязана'}{part.bind[p.id]&&<button aria-label={'Отвязать '+p.label} onClick={()=>{const bind={...part.bind};delete bind[p.id];onChange(part.id,{bind})}}>×</button>}</li>)}</ul>
   <button className="danger" onClick={()=>onRemove(part.id)}>Удалить компонент</button></div>}
  {message&&<p role="status">{message}</p>}
  <label>Другая библиотека (screen-library.json)<input type="file" accept=".json,application/json" onChange={e=>open(e.target.files[0])}/></label>
  {shown&&<PartPreview def={shown} assets={[...(library?.assets||[]),...documentAssets]}/>}
  {Object.entries(tree.folders).map(([n,node])=><Folder key={n} name={n} node={node} chosen={chosen} onChoose={setChosen} onAdd={add}/>)}{tree.items.map(c=><div key={c.id} className="svg-asset-card" onClick={()=>setChosen(c.id)}><span>{c.title}</span><button aria-label={'Добавить '+c.title} onClick={e=>{e.stopPropagation();add(c)}}>+</button></div>)}</div>;
}
