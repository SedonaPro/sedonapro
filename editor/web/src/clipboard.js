import{validateGraph}from'./project.js';
export function copySelection(graph,ids){const selected=new Set(ids);return validateGraph({...graph,nodes:graph.nodes.filter(n=>selected.has(n.id)),edges:graph.edges.filter(e=>selected.has(e.source)&&selected.has(e.target))})}
export function pasteSelection(graph,clipboard,position={x:64,y:64},parent){
 const clip=validateGraph(clipboard);if(!clip.nodes.length)return {graph,ids:[]};if(graph.nodes.length+clip.nodes.length>2000)throw Error('Не более 2000 компонентов');
 const minX=Math.min(...clip.nodes.map(n=>n.x)),minY=Math.min(...clip.nodes.map(n=>n.y)),ids=new Map(),names=new Set(graph.nodes.map(n=>n.name)),newValues={...graph.values};let number=1;
 const nodes=clip.nodes.map(n=>{while(names.has('blk'+number))number++;const name='blk'+number++;names.add(name);const id='p'+crypto.randomUUID().replaceAll('-','');ids.set(n.id,id);newValues[id]={...clip.values[n.id]};const {coordinateSource,...rest}=n;return {...rest,id,name,x:n.x-minX+position.x,y:n.y-minY+position.y,...(Number.isInteger(parent)?{parent}:{})}});
 const edges=clip.edges.map(e=>({...e,id:'l'+crypto.randomUUID().replaceAll('-',''),source:ids.get(e.source),target:ids.get(e.target)}));
 const next=validateGraph({...graph,nodes:[...graph.nodes,...nodes],edges:[...graph.edges,...edges],values:newValues});return {graph:next,ids:[...ids.values()]};
}
