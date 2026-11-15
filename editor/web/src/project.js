/** Canonical project model. No renderer objects, execution code or device credentials. */
import{validateUi}from'./uiDocument.js';
export const FORMAT='mcs-sedona-project';
const forbidden=new Set(['__proto__','prototype','constructor']);
const ident=x=>typeof x==='string'&&/^[A-Za-z][A-Za-z0-9_-]{0,63}$/.test(x)&&!forbidden.has(x);
const str=(v,n=256)=>typeof v==='string'&&v.length<=n;
const plain=v=>v!==null&&typeof v==='object'&&!Array.isArray(v);
const assert=(b,m)=>{if(!b)throw Error(m)};
export const scalar=t=>String(t).replace(/^sys::/,'');
export const canInput=p=>!p.readonly&&p.name!=='meta'&&(p.input??p.name!=='out');
export const canOutput=p=>p.name!=='meta'&&p.output!==false;
export function connectionError(graph,c){
 const a=graph.nodes.find(n=>n.id===c.source),b=graph.nodes.find(n=>n.id===c.target);
 if(!a||!b)return 'Компонент не найден';
 const out=a.ports.find(p=>p.id===c.sourceSlot),input=b.ports.find(p=>p.id===c.targetSlot);
 if(!out||!input)return 'Порт не найден';
 if(!canOutput(out)||!canInput(input))return 'В этот порт нельзя записывать связь';
 if(scalar(out.type)!==scalar(input.type))return `Несовместимые типы: ${scalar(out.type)} → ${scalar(input.type)}`;
 if(c.source===c.target&&c.sourceSlot===c.targetSlot)return 'Порт нельзя соединить с самим собой';
 if(graph.edges.some(e=>e.target===c.target&&e.targetSlot===c.targetSlot))return 'Вход уже связан. Сначала удалите прежнюю связь';
 return null;
}
export function validateValue(port,value){
 const t=scalar(port.type),s=String(value).trim();
 if(port.readonly||!canInput(port))throw Error('Поле доступно только для чтения');
 if(t==='bool'){if(s!=='true'&&s!=='false')throw Error('Ожидается true или false');return s;}
 if(['byte','short','int','long'].includes(t)){
  if(!/^-?\d+$/.test(s))throw Error('Ожидается целое число');const n=BigInt(s),ranges={byte:[0n,255n],short:[0n,65535n],int:[-2147483648n,2147483647n],long:[-9223372036854775808n,9223372036854775807n]};
  const [lo,hi]=ranges[t];if(n<lo||n>hi)throw Error('Число вне диапазона типа '+t);return n.toString();
 }
 if(t==='float'||t==='double'){if(!s||!Number.isFinite(Number(s)))throw Error('Ожидается конечное число');const n=Number(s);if(t==='float'&&!Number.isFinite(Math.fround(n)))throw Error('Число вне диапазона float');return s;}
 throw Error('Редактирование типа '+t+' пока не поддерживается');
}
export function validateGraph(input){
 assert(plain(input),'Некорректная схема');assert(Array.isArray(input.nodes)&&input.nodes.length<=2000,'Не более 2000 блоков');assert(Array.isArray(input.edges)&&input.edges.length<=6000,'Не более 6000 связей');
 const ids=new Set();const nodes=input.nodes.map(n=>{
  assert(plain(n)&&ident(n.id)&&!ids.has(n.id),'Некорректный или повторный ID блока');ids.add(n.id);
  assert(str(n.name,128)&&str(n.type,160),'Некорректное имя/тип');assert(Number.isFinite(n.x)&&Number.isFinite(n.y)&&Math.abs(n.x)<=1e6&&Math.abs(n.y)<=1e6,'Некорректные координаты');
  assert(Array.isArray(n.ports)&&n.ports.length<=128,'Не более 128 портов в блоке');const seen=new Set(),seenNames=new Set();const ports=n.ports.map(p=>{assert(plain(p)&&Number.isInteger(p.id)&&p.id>=0&&p.id<=255&&!seen.has(p.id),'Некорректный ID порта');seen.add(p.id);assert(str(p.name,64)&&p.name.length>0&&!seenNames.has(p.name)&&!forbidden.has(p.name)&&str(p.type,160)&&p.type.length>0,'Некорректное имя/тип порта');seenNames.add(p.name);const v={id:p.id,name:p.name,type:p.type};for(const k of ['input','output','readonly','config','hidden','summary'])if(typeof p[k]==='boolean')v[k]=p[k];return v;});
  return {id:n.id,name:n.name,type:n.type,x:n.x,y:n.y,ports,...(str(n.kitPart,128)?{kitPart:n.kitPart}:{}),...(Number.isInteger(n.parent)?{parent:n.parent}:{}),...(n.coordinateSource==='meta'?{coordinateSource:'meta'}:{})};
 });
 const graph={nodes,edges:[],values:{},synthetic:input.synthetic===true};const edgeIds=new Set();
 // Imported existing device links are validated for endpoints/types; a readonly target is retained
 // for inspection, while new connections always use connectionError's stricter editing policy.
 for(const e of input.edges){assert(plain(e)&&ident(e.id)&&!edgeIds.has(e.id),'Некорректный ID связи');edgeIds.add(e.id);const a=nodes.find(n=>n.id===e.source),b=nodes.find(n=>n.id===e.target),ap=a?.ports.find(p=>p.id===e.sourceSlot),bp=b?.ports.find(p=>p.id===e.targetSlot);assert(ap&&bp,'Связь ссылается на отсутствующий порт');assert(scalar(ap.type)===scalar(bp.type),'Типы в связи не совпадают');assert(!graph.edges.some(x=>x.target===e.target&&x.targetSlot===e.targetSlot),'Несколько связей на один вход');graph.edges.push({id:e.id,source:e.source,target:e.target,sourceSlot:e.sourceSlot,targetSlot:e.targetSlot});}
 if(plain(input.values))for(const n of nodes){const source=input.values[n.id];if(!plain(source))continue;const fields={};for(const p of n.ports)if(Object.hasOwn(source,p.name)){const v=source[p.name];assert(['string','number','boolean'].includes(typeof v)&&String(v).length<=4096,'Некорректное значение');assert(typeof v!=='number'||Number.isFinite(v),'Неконечное число');fields[p.name]=String(v);}graph.values[n.id]=fields;}
 if(plain(input.origin)){const o=input.origin;graph.origin={};if(str(o.platformId,160))graph.origin.platformId=o.platformId;if(str(o.capturedAt,64))graph.origin.capturedAt=o.capturedAt;if(Array.isArray(o.parts)&&o.parts.length<=256&&o.parts.every(x=>str(x,128)))graph.origin.parts=[...o.parts];}
 if(input.ui)graph.ui=validateUi(input.ui);
 return graph;
}
export function exportProject(graph,title='Sedona project'){return JSON.stringify({format:FORMAT,version:1,title:String(title).slice(0,128),savedAt:new Date().toISOString(),graph:validateGraph(graph)},null,2);}
export function importProject(text){assert(typeof text==='string'&&new TextEncoder().encode(text).length<=8*1024*1024,'Файл больше 8 MiB');const p=JSON.parse(text);assert(p.format===FORMAT&&p.version===1,'Неизвестный формат или версия проекта');return {title:str(p.title,128)?p.title:'Sedona project',graph:validateGraph(p.graph)};}
export function historyState(graph){return {present:graph,past:[],future:[],saved:graph};}
export function historyReducer(s,a){
 if(a.type==='reset')return historyState(a.graph);
 if(a.type==='saved')return {...s,saved:s.present};
 if(a.type==='undo'){if(!s.past.length)return s;return {...s,present:s.past.at(-1),past:s.past.slice(0,-1),future:[s.present,...s.future].slice(0,50)};}
 if(a.type==='redo'){if(!s.future.length)return s;return {...s,present:s.future[0],past:[...s.past,s.present].slice(-50),future:s.future.slice(1)};}
 if(a.type==='edit'){const next=a.change(s.present);if(next===s.present)return s;return {...s,present:next,past:[...s.past,s.present].slice(-50),future:[]};}
 return s;
}
export function addBlock(graph,template){
 assert(graph.nodes.length<2000,'Достигнут предел 2000 блоков');let i=1;while(graph.nodes.some(n=>n.name===`blk${i}`))i++;const id='p'+crypto.randomUUID().replaceAll('-',''),name=`blk${i}`,last=graph.nodes.at(-1);const n={id,name,type:template.type,kitPart:template.kitPart,ports:template.ports.map(p=>({...p})),x:last?last.x+270:40,y:last?last.y:40};return {...graph,nodes:[...graph.nodes,n],values:{...graph.values,[id]:{...template.defaults}}};
}
export function connectPorts(graph,c){const error=connectionError(graph,c);if(error)throw Error(error);return {...graph,edges:[...graph.edges,{...c,id:'l'+crypto.randomUUID().replaceAll('-','')}]};}
export function deleteBlock(graph,id){const next={...graph.values};delete next[id];return {...graph,nodes:graph.nodes.filter(n=>n.id!==id),edges:graph.edges.filter(e=>e.source!==id&&e.target!==id),values:next};}
