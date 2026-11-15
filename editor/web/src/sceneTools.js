export const WIDGET_MIME='application/x-mcs-widget';
export const widgetLibrary=[
 {id:'number',appearance:'compact',width:150,title:'Числовое значение',kind:'value',unit:'',decimals:1},
 {id:'temperature',appearance:'compact',width:145,title:'Температура',kind:'value',unit:'°C',decimals:1},
 {id:'pressure',title:'Давление',kind:'value',unit:'Па',decimals:0},
 {id:'percent',title:'Процент',kind:'value',unit:'%',decimals:1},
 {id:'status',appearance:'compact',width:150,title:'Состояние / индикатор',kind:'indicator',unit:'',decimals:0},
 {id:'setpoint',title:'Редактируемая уставка',kind:'setpoint',unit:'',decimals:1},
 {id:'bare',title:'Только значение',kind:'value',appearance:'value',width:90,showLabel:false,showQuality:false,unit:'',decimals:1},
 {id:'lamp',title:'Сигнальная лампа',kind:'indicator',appearance:'lamp',width:40,showLabel:false,showQuality:false,unit:'',decimals:0},
 {id:'toggle-button',title:'Кнопка Вкл / Выкл',kind:'toggle',appearance:'card',width:150,unit:'',decimals:0},
 {id:'toggle',title:'Переключатель',kind:'toggle',appearance:'compact',width:160,unit:'',decimals:0},
 {id:'on',title:'Кнопка Включить',kind:'button',command:true,appearance:'compact',width:140,unit:'',decimals:0},
 {id:'off',title:'Кнопка Выключить',kind:'button',command:false,appearance:'compact',width:140,unit:'',decimals:0},
 {id:'bar',title:'Линейная шкала',kind:'bar',appearance:'compact',width:180,min:0,max:100,unit:'%',decimals:1},
 {id:'gauge',title:'Круговая шкала',kind:'gauge',width:140,min:0,max:100,unit:'%',decimals:1},
 {id:'slider',title:'Ползунок уставки',kind:'slider',width:180,min:0,max:100,unit:'',decimals:1},
 {id:'trend',title:'Многоканальный тренд',kind:'trend',width:420,height:260,windowSeconds:300,sampleSeconds:1,autoScale:true,legend:true,series:[],unit:'',decimals:1},
 {id:'bars',title:'Сравнение значений',kind:'bars',width:360,height:220,min:0,max:100,legend:true,series:[],unit:'',decimals:1},
 {id:'table',title:'Таблица переменных',kind:'table',width:480,height:240,series:[],unit:'',decimals:2},
 {id:'text',title:'Текст / подпись',kind:'text',appearance:'value',width:180,showQuality:false,unit:'',decimals:0}
];
export const clamp=(v,min=0,max=100)=>Math.max(min,Math.min(max,v));
export function positions(rects,action,options={}){
 if(rects.length<(options.reference?1:2))return {};
 const ref=options.reference,offsetX=options.offsetX||0,offsetY=options.offsetY||0,left=(ref?ref.x:Math.min(...rects.map(r=>r.x)))+offsetX,right=(ref?ref.x+ref.w:Math.max(...rects.map(r=>r.x+r.w)))-offsetX,top=(ref?ref.y:Math.min(...rects.map(r=>r.y)))+offsetY,bottom=(ref?ref.y+ref.h:Math.max(...rects.map(r=>r.y+r.h)))-offsetY;
 const out={};
 if(action==='joinRight'||action==='joinLeft'){const anchor=options.reference||rects[0];let cursor=action==='joinRight'?anchor.x+anchor.w:anchor.x;for(const r of rects){if(r.id===anchor.id)continue;const x=action==='joinRight'?cursor:cursor-r.w;out[r.id]={x:clamp(x,0,Math.max(0,100-r.w)),y:clamp(anchor.y,0,Math.max(0,100-r.h))};cursor=action==='joinRight'?x+r.w:x;}return out;}

 if(action.startsWith('distribute')){
  if(rects.length<3)return out;const horizontal=action==='distributeX',axis=horizontal?'x':'y',size=horizontal?'w':'h',ordered=[...rects].sort((a,b)=>a[axis]-b[axis]),first=ordered[0],last=ordered.at(-1),gap=(last[axis]+last[size]-first[axis]-ordered.reduce((s,r)=>s+r[size],0))/(ordered.length-1);let cursor=first[axis];
  for(const r of ordered){out[r.id]={[axis]:cursor};cursor+=r[size]+gap;}return out;
 }
 for(const r of rects){if(r.id===options.anchorId)continue;const p={};if(action==='left')p.x=left;if(action==='right')p.x=right-r.w;if(action==='centerX')p.x=(left+right-r.w)/2;if(action==='top')p.y=top;if(action==='bottom')p.y=bottom-r.h;if(action==='centerY')p.y=(top+bottom-r.h)/2;if(p.x!==undefined)p.x=clamp(p.x,0,Math.max(0,100-r.w));if(p.y!==undefined)p.y=clamp(p.y,0,Math.max(0,100-r.h));out[r.id]=p;}return out;
}
export function patchItems(view,patches){return {...view,objects:(view.objects||[]).map(o=>patches[o.id]?{...o,...patches[o.id]}:o),widgets:(view.widgets||[]).map(o=>patches[o.id]?{...o,...patches[o.id]}:o)};}
