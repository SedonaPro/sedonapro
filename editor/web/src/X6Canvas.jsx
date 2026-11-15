import React,{useEffect,useRef} from 'react';
import {Graph} from '@antv/x6';
import {values} from './model';
export default function X6Canvas({graph,onSelect,onReady,onVisible}){
 const element=useRef();
 useEffect(()=>{
  const engine=new Graph({container:element.current,autoResize:true,grid:{size:20,visible:true},panning:true,mousewheel:{enabled:true,minScale:.1,maxScale:2},connecting:{allowBlank:false,allowNode:false},interacting:{edgeMovable:false,magnetConnectable:false},async:false});
  const unsub=[];
  engine.batchUpdate(()=>{
   for(const n of graph.nodes){const markup=[{tagName:'rect',selector:'body'},{tagName:'rect',selector:'head'},{tagName:'text',selector:'title'},{tagName:'text',selector:'kind'}];const attrs={body:{fill:'#fff',stroke:'#b5c9c6',rx:6,ry:6},head:{x:1,y:1,width:218,height:27,fill:'#e8f0ed',stroke:'none',rx:5,ry:5},title:{x:10,y:18,text:n.name,fontFamily:'Arial',fontWeight:600,fontSize:11,fill:'#193b38'},kind:{x:10,y:41,text:n.type,fontFamily:'Arial',fontSize:10,fill:'#7c8989'}};
    const ports=[];n.ports.forEach((p,i)=>{const y=58+i*21;markup.push({tagName:'text',selector:`s${i}`},{tagName:'text',selector:`v${i}`});attrs[`s${i}`]={x:10,y:y+8,text:p.name,fontSize:11,fontFamily:'Arial',fill:'#54706c'};attrs[`v${i}`]={x:210,y:y+8,text:values.get(n.id)[p.name]??'—',textAnchor:'end',fontSize:11,fontFamily:'Arial',fill:'#193b38'};ports.push({id:`${p.id}-in`,group:'fixed',args:{x:0,y:y+4}},{id:`${p.id}-out`,group:'fixed',args:{x:220,y:y+4}});});
    for(const [key,a] of Object.entries(attrs))if(key!=='body'&&key!=='head'){a.refX=0;a.refY=0;if(!a.textAnchor)a.textAnchor='start';}
    const node=engine.addNode({id:n.id,x:n.x,y:n.y,width:220,height:52+n.ports.length*21,markup,attrs,ports:{groups:{fixed:{position:'absolute',attrs:{circle:{r:3,magnet:true,stroke:'#6c9b94',fill:'#f8fcfa'}}}},items:ports}});
    let previous=values.get(n.id);unsub.push(values.subscribe(n.id,()=>{const v=values.get(n.id),patch={};for(let i=0;i<n.ports.length;i++)if(v[n.ports[i].name]!==previous[n.ports[i].name])patch[`v${i}`]={text:v[n.ports[i].name]??'—'};previous=v;if(Object.keys(patch).length)node.attr(patch);}));
   }
   for(const e of graph.edges)engine.addEdge({id:e.id,source:{cell:e.source,port:`${e.sourceSlot}-out`},target:{cell:e.target,port:`${e.targetSlot}-in`},router:{name:'orth'},attrs:{line:{stroke:'#779d9a',strokeWidth:1.3,targetMarker:{name:'block',size:4}}}});
  });
  engine.zoomTo(.72);engine.translate(0,0);let selected;
  engine.on('node:click',({node})=>{if(selected)selected.attr('body/stroke','#b5c9c6');node.attr('body/stroke','#176d61');selected=node;onSelect(node.id)});
  let notifyTimer;const notify=()=>{clearTimeout(notifyTimer);notifyTimer=setTimeout(()=>{const r=engine.getGraphArea();onVisible(engine.getNodes().filter(n=>{const b=n.getBBox();return b.x+b.width>=r.x&&b.x<=r.x+r.width&&b.y+b.height>=r.y&&b.y<=r.y+r.height}).map(n=>n.id));},80)};engine.on('scale',notify);engine.on('translate',notify);engine.on('node:change:position',notify);notify();
  let alive=true;requestAnimationFrame(()=>requestAnimationFrame(()=>alive&&onReady()));
  return()=>{alive=false;clearTimeout(notifyTimer);unsub.forEach(f=>f());engine.dispose()};
 },[graph,onSelect,onReady,onVisible]);
 return <div className="x6-canvas" ref={element}/>;
}
