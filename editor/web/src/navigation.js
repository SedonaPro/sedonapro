// A view over the full graph: never changes the project or device structure.
export function navigationIndex(graph){
 const byId=new Map(graph.nodes.map(n=>[n.id,n])),children=new Map();
 for(const n of graph.nodes){const parent=Number.isInteger(n.parent)?`n${n.parent}`:null;if(parent&&parent!==n.id&&byId.has(parent)){if(!children.has(parent))children.set(parent,[]);children.get(parent).push(n);}}
 function path(id){const result=[],seen=new Set();let n=byId.get(id);while(n&&!seen.has(n.id)){seen.add(n.id);result.unshift(n);n=Number.isInteger(n.parent)?byId.get(`n${n.parent}`):null;}return result;}
 return {byId,children,path};
}
export function navigationView(graph,scope='',query=''){
 const index=navigationIndex(graph),q=query.trim().toLocaleLowerCase();
 if(!scope&&!q)return {graph,externalLinks:0};
 let nodes=scope&&!q?(index.children.get(scope)||[]):graph.nodes;
 if(q)nodes=nodes.filter(n=>`${n.name} ${n.type} ${n.kitPart||''} ${index.path(n.id).map(p=>p.name).join('/')}`.toLocaleLowerCase().includes(q));
 const ids=new Set(nodes.map(n=>n.id)),edges=graph.edges.filter(e=>ids.has(e.source)&&ids.has(e.target));
 const externalLinks=graph.edges.filter(e=>ids.has(e.source)!==ids.has(e.target)).length;
 const rowHeight=Math.max(160,80+Math.max(...nodes.map(x=>x.ports.length),0)*21);
 nodes=nodes.map((n,i)=>n.coordinateSource==='meta'?n:({...n,x:30+(i%4)*290,y:25+Math.floor(i/4)*rowHeight}));
 return {graph:{...graph,nodes,edges},externalLinks};
}
export function treeRows(graph,expanded=new Set(),query=''){
 const index=navigationIndex(graph),q=query.trim().toLocaleLowerCase(),allowed=new Set(),matches=new Set();
 if(q)for(const n of graph.nodes)if(`${n.name} ${n.type} ${n.kitPart||''} ${index.path(n.id).map(p=>p.name).join('/')}`.toLocaleLowerCase().includes(q)){matches.add(n.id);for(const p of index.path(n.id))allowed.add(p.id)}
 const roots=graph.nodes.filter(n=>!Number.isInteger(n.parent)||!index.byId.has(`n${n.parent}`)||`n${n.parent}`===n.id),seen=new Set(),rows=[];
 function walk(n,depth,parent,visible){if(seen.has(n.id))return;seen.add(n.id);const kids=index.children.get(n.id)||[],included=!q||allowed.has(n.id);if(visible&&included)rows.push({node:n,depth,parent,branch:kids.length>0||/::(Folder|App)$/.test(n.type),expanded:q?true:expanded.has(n.id),match:matches.has(n.id)});for(const child of kids)walk(child,depth+1,n.id,visible&&included&&(!!q||expanded.has(n.id)))}
 for(const n of roots)walk(n,1,null,true);for(const n of graph.nodes)if(!seen.has(n.id))walk(n,1,null,true);return rows;
}
