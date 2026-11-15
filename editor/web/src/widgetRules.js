export const widgetKinds=['value','indicator','setpoint','toggle','button','bar','gauge','slider','text','trend','bars','table'];
export const boolKinds=['indicator','toggle','button'];
export const writeKinds=['setpoint','toggle','button','slider'];
export function widgetOptions(w){
 const out={};
 if(w.appearance!==undefined){if(!['card','compact','value','lamp'].includes(w.appearance))throw Error('Неизвестный вид оформления');out.appearance=w.appearance;}
 for(const [key,min,max]of [['width',40,900],['height',100,600],['windowSeconds',10,3600],['sampleSeconds',1,60],['fontSize',10,72],['min',-1e9,1e9],['max',-1e9,1e9]])if(w[key]!==undefined){if(!Number.isFinite(w[key])||w[key]<min||w[key]>max)throw Error('Недопустимый параметр '+key);out[key]=w[key];}
 if((w.max??100)<=(w.min??0))throw Error('Максимум должен быть больше минимума');
 for(const key of ['showLabel','showUnit','showQuality','border','background','command','autoScale','legend'])if(w[key]!==undefined){if(typeof w[key]!=='boolean')throw Error('Ожидается bool: '+key);out[key]=w[key];}
 if(w.series!==undefined){if(!Array.isArray(w.series)||w.series.length>8)throw Error('До 8 каналов');out.series=w.series.map(s=>{if(!s||typeof s.path!=='string'||s.path.length>2048||s.path.split('/').some(p=>!p||p==='..')||typeof s.slot!=='string'||!s.slot||s.slot.length>128||s.slot==='meta'||/password|secret|cred|^key$/i.test(s.slot)||typeof s.type!=='string'||s.type.length>160)throw Error('Некорректная переменная канала');if(s.color!==undefined&&!/^#[0-9a-f]{6}$/i.test(s.color))throw Error('Цвет: #RRGGBB');return {path:s.path,slot:s.slot,type:s.type,label:String(s.label||s.slot).slice(0,128),unit:String(s.unit||'').slice(0,32),...(s.color?{color:s.color}:{})}})}
 return out;
}
export function fraction(value,w){const n=Number(value),min=w.min??0,max=w.max??100;return Number.isFinite(n)?Math.max(0,Math.min(1,(n-min)/(max-min))):0;}
export function validateWidgets(list,limit=64){if(!Array.isArray(list)||list.length>limit)throw Error('Не более '+limit+' виджетов на экране');const seen=new Set();return list.map(w=>{if(!w||typeof w.id!=='string'||!/^[A-Za-z0-9_-]{1,64}$/.test(w.id)||seen.has(w.id)||!widgetKinds.includes(w.kind)||typeof w.path!=='string'||w.path.length>2048||w.path.split('/').some(x=>x==='..'||!x)||typeof w.slot!=='string'||!w.slot||w.slot==='meta'||/password|secret|cred|^key$/i.test(w.slot)||typeof w.type!=='string'||w.type.length>160||![w.x,w.y].every(x=>Number.isFinite(x)&&x>=0&&x<=100)||!Number.isInteger(w.decimals)||w.decimals<0||w.decimals>6)throw Error('Некорректный виджет');seen.add(w.id);return {id:w.id,kind:w.kind,label:String(w.label||w.slot).slice(0,128),path:w.path,slot:w.slot,type:w.type,x:w.x,y:w.y,unit:String(w.unit||'').slice(0,32),decimals:w.decimals,...widgetOptions(w)};});}
