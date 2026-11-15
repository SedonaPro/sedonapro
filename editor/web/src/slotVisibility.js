// Preserve every linked endpoint so filtering can never leave dangling wires.
export function slotView(graph,showAll=false){
 if(showAll)return graph;const linked=new Set();for(const e of graph.edges){linked.add(`${e.source}:${e.sourceSlot}`);linked.add(`${e.target}:${e.targetSlot}`)}
 return {...graph,nodes:graph.nodes.map(n=>({...n,ports:n.ports.filter(p=>linked.has(`${n.id}:${p.id}`)||(!p.hidden&&p.name!=='meta'&&(p.summary??!p.config)))}))};
}
