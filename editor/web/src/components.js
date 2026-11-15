// Graphic components of the shared "mcs-sedona-ui" package, version 3.
// A component is a small picture of its own: symbols and widgets placed in per cent of its box,
// whose values are named points instead of slots (inside, a binding has the path "." and the
// point's id as its slot). A part placed on a screen says which slot feeds which point.
// Sedona Studio reads and writes the same shape (storage.ScreenStore).
import{validateWidgets}from'./widgetRules.js';
import{validateObjects}from'./svgObjects.js';
const id=x=>typeof x==='string'&&/^[A-Za-z0-9_-]{1,64}$/.test(x);
const text=(x,max)=>typeof x==='string'&&x.length<=max;
const slot=(x,max)=>typeof x==='string'&&x.length>0&&x.length<=max&&x!=='meta'&&!/password|secret|cred|^key$/i.test(x);
const check=(ok,message)=>{if(!ok)throw Error(message)};
const numeric=['float','double','int','long','short','byte'];
export const UNBOUND='?';
export function accepts(point,type){const t=String(type).replace('sys::','');return point.kind==='bool'?t==='bool':point.kind==='number'?numeric.includes(t):true;}
export function validateComponents(value,library,limit=64){
 check(Array.isArray(value)&&value.length<=limit,'Слишком много графических компонентов');const ids=new Set();
 return value.map(c=>{
  check(c&&id(c.id)&&!ids.has(c.id)&&text(c.title,128)&&[c.width,c.height].every(n=>Number.isFinite(n)&&n>0&&n<=2000)&&Array.isArray(c.points)&&c.points.length<=16,'Некорректный графический компонент');ids.add(c.id);
  const names=new Set(),points=c.points.map(p=>{check(p&&id(p.id)&&slot(p.id,64)&&!names.has(p.id)&&text(p.label,128)&&['bool','number','any'].includes(p.kind)&&Array.isArray(p.slots)&&p.slots.length<=8&&p.slots.every(s=>slot(s,128)),'Некорректная точка компонента');names.add(p.id);return {id:p.id,label:p.label,kind:p.kind,slots:[...p.slots]};});
  const inside=b=>check(!b||b.path==='.'&&names.has(b.slot),'Привязка внутри компонента должна называть его точку');
  const out={id:c.id,title:c.title,width:c.width,height:c.height,points};
  if(c.objects!==undefined){check(Array.isArray(c.objects)&&c.objects.length<=16,'Не более 16 картинок в компоненте');out.objects=validateObjects(c.objects,library);for(const o of out.objects){inside(o.binding);inside(o.fault);}}
  if(c.widgets!==undefined){out.widgets=validateWidgets(c.widgets,16);for(const w of out.widgets){check(!['trend','bars','table'].includes(w.kind),'Графики в компоненте не поддерживаются');if(w.kind!=='text')inside(w);}}
  return out;});
}
export function validateParts(value,components){
 check(Array.isArray(value)&&value.length<=64,'Не более 64 компонентов на экране');const ids=new Set();
 return value.map(p=>{
  const def=p&&components.find(c=>c.id===p.componentId);
  check(p&&id(p.id)&&!ids.has(p.id)&&def&&text(p.label,128)&&[p.x,p.y].every(n=>Number.isFinite(n)&&n>=0&&n<=100)&&p.bind&&typeof p.bind==='object'&&!Array.isArray(p.bind),'Некорректный размещённый компонент');ids.add(p.id);
  const bind={};for(const [name,b]of Object.entries(p.bind)){const point=def.points.find(x=>x.id===name);
   check(point&&b&&text(b.path,2048)&&b.path.length&&!b.path.split('/').some(x=>!x||x==='..')&&slot(b.slot,128)&&text(b.type,160)&&b.type.length&&accepts(point,b.type),'Некорректная привязка точки компонента');bind[name]={path:b.path,slot:b.slot,type:b.type};}
  return {id:p.id,componentId:p.componentId,label:p.label,x:p.x,y:p.y,bind};});
}
// The symbols and widgets a screen shows for its parts, in per cent of the screen, each reading the
// slot bound to its point. An unbound point gets a path no component has, so it shows as missing;
// an unbound fault signal is simply left out.
export function partItems(view,components,canvas){
 const objects=[],widgets=[];
 for(const part of view?.parts||[]){const def=(components||[]).find(c=>c.id===part.componentId);if(!def)continue;
  const real=b=>b&&(part.bind[b.slot]||{path:UNBOUND,slot:b.slot,type:b.type}),at=o=>({x:part.x+o.x/100*def.width/canvas.width*100,y:part.y+o.y/100*def.height/canvas.height*100});
  for(const o of def.objects||[]){const item={...o,...at(o),id:part.id+'~'+o.id,label:part.label};if(o.binding)item.binding=real(o.binding);if(o.fault){if(part.bind[o.fault.slot])item.fault=part.bind[o.fault.slot];else delete item.fault;}objects.push(item);}
  for(const w of def.widgets||[])widgets.push({...w,...at(w),id:part.id+'~'+w.id,...(w.kind==='text'?{}:real(w))});}
 return {objects,widgets};
}
// The slots of one component that feed the points: for each point the first suggested name the
// component has with a fitting type. choices are viewBindings.bindingChoices(graph,folder).
export function autoBind(choices,def,path){
 const node=choices.find(c=>c.relative===path)?.node,bind={};if(!node)return bind;
 for(const point of def.points)for(const name of point.slots){const port=node.ports.find(p=>p.name===name&&accepts(point,p.type));if(port){bind[point.id]={path,slot:port.name,type:port.type};break;}}
 return bind;
}
