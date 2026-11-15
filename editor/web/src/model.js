export const values = {
 state: {}, listeners: new Map(),
 get(id) { return this.state[id] || EMPTY },
 subscribe(id, fn) { if (!this.listeners.has(id)) this.listeners.set(id,new Set()); this.listeners.get(id).add(fn); return () => { this.listeners.get(id)?.delete(fn); if (!this.listeners.get(id)?.size) this.listeners.delete(id); } },
 set(next) { this.state=next; for(const [,set] of this.listeners) for(const fn of set) fn() },
};
const EMPTY={};
export function demoGraph(count=100) {
 const names=['in1','in2','setpoint','kp','ki','min','max','out'];
 const nodes=Array.from({length:count},(_,i)=>({id:`n${i}`,name:`AHU ${String(Math.floor(i/10)+1).padStart(2,'0')} · ${i%3===0?'PID':i%3===1?'Scale':'Limit'} ${i+1}`,type:`control::${i%3===0?'Pid':'Float'}`,ports:names.map((name,id)=>({id,name,type:'float'})),x:(i%8)*290+30,y:Math.floor(i/8)*280+25}));
 const edges=nodes.slice(1).map((n,i)=>({id:`e${i}`,source:nodes[i].id,sourceSlot:7,target:n.id,targetSlot:0}));
 for(let i=10;i<count;i+=10)edges.push({id:`feedback${i}`,source:`n${i}`,sourceSlot:7,target:`n${i-5}`,targetSlot:1});
 const initial={};for(const n of nodes)initial[n.id]=Object.fromEntries(names.map((key,i)=>[key,i===2?'22.0':i===7?'48.5':i===6?'100':i===5?'0':(i+1).toFixed(1)]));
 return {nodes,edges,values:initial,synthetic:true};
}
export function devicePosition(meta){
 if(!/^-?\d+$/.test(String(meta)))return null;const v=Number(meta);if(!Number.isInteger(v)||v< -2147483648||v>4294967295)return null;
 // Ontrol BSoxVirtualComponent: x = meta >>> 24, y = meta >>> 16.
 return {x:((v>>>24)&255)*16,y:((v>>>16)&255)*16};
}
export function fromDevice(g){return {nodes:g.nodes.map((n,i)=>{const meta=g.values?.[`n${n.id}`]?.meta,p=devicePosition(meta);return {...n,id:`n${n.id}`,x:p?.x??(i%8)*290+30,y:p?.y??Math.floor(i/8)*300+25,...(p?{coordinateSource:'meta'}:{})}}),edges:g.edges.map((e,i)=>({...e,id:`real${i}`,source:`n${e.source}`,target:`n${e.target}`})),values:g.values,origin:g.origin,synthetic:false};}
export function tick(graph,phase){const next={...values.state};for(let i=0;i<Math.min(100,graph.nodes.length);i++){const n=graph.nodes[i];next[n.id]={...next[n.id],out:(48+8*Math.sin(phase/5+i/7)).toFixed(2)};}values.set(next);}
