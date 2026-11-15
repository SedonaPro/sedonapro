import React,{useMemo,useState,useEffect,useRef}from'react';
import{navigationIndex,treeRows}from'./navigation';
export default function NavigationPanel({graph,scope,query,onScope,onQuery,onSelect,selected,shown,externalLinks,editing}){
 const index=useMemo(()=>navigationIndex(graph),[graph]),[expanded,setExpanded]=useState(()=>new Set(index.path(scope).map(n=>n.id))),tree=useRef();
 useEffect(()=>{setExpanded(old=>new Set([...old,...index.path(scope).map(n=>n.id)]))},[scope,index]);
 const rows=useMemo(()=>treeRows(graph,expanded,query),[graph,expanded,query]),active=selected||scope||rows[0]?.node.id;
 function toggle(id){setExpanded(old=>{const next=new Set(old);next.has(id)?next.delete(id):next.add(id);return next})}
 function open(id){onQuery('');onScope(id);setExpanded(old=>new Set([...old,...index.path(id).map(n=>n.id)]))}
 function select(row){if(row.branch)open(row.node.id);else{if(!editing){onQuery('');onScope(row.parent||'')}onSelect(row.node.id)}}
 function focus(id){tree.current?.querySelector(`[data-tree-id="${id}"]`)?.focus()}
 function key(e,row,i){if(e.target!==e.currentTarget)return;const id=row.node.id;if(e.key==='ArrowDown'){e.preventDefault();focus(rows[Math.min(i+1,rows.length-1)]?.node.id)}else if(e.key==='ArrowUp'){e.preventDefault();focus(rows[Math.max(0,i-1)]?.node.id)}else if(e.key==='ArrowRight'){e.preventDefault();if(row.branch&&!row.expanded)toggle(id);else if(row.branch)focus(rows[i+1]?.node.id)}else if(e.key==='ArrowLeft'){e.preventDefault();if(row.branch&&row.expanded&&!query)toggle(id);else if(row.parent)focus(row.parent)}else if(e.key==='Enter'||e.key===' '){e.preventDefault();select(row)}else if(e.key==='Home'){e.preventDefault();focus(rows[0]?.node.id)}else if(e.key==='End'){e.preventDefault();focus(rows.at(-1)?.node.id)}}
 return <div className="panel navigation-panel"><div className="eyebrow">НАВИГАЦИЯ ПО СХЕМЕ</div><h3>Дерево компонентов</h3>
 <label>Поиск по имени, типу или киту<input value={query} onChange={e=>onQuery(e.target.value)} maxLength={160}/></label>
 <button onClick={()=>{onScope('');onQuery('')}}>Показать всю схему</button>
 <div className="button-row tree-actions"><button onClick={()=>setExpanded(new Set(index.children.keys()))}>Развернуть всё</button><button onClick={()=>setExpanded(new Set())}>Свернуть всё</button></div>
 {editing&&<p className="note">В режиме редактирования схема показывается целиком. Дерево помогает выбрать компонент.</p>}
 <div className="breadcrumbs"><button onClick={()=>open('')}>Корень</button>{index.path(scope).map(n=><button key={n.id} onClick={()=>open(n.id)}>{n.name}</button>)}</div>
 <p className="muted" role="status">В дереве: {graph.nodes.length} · на схеме: {shown}{externalLinks>0?` · связей за пределами вида: ${externalLinks}`:''}</p>
 <div ref={tree} className="component-tree" role="tree" aria-label="Дерево компонентов">{rows.map((row,i)=>{const n=row.node;return <div role="treeitem" aria-label={n.name} aria-level={row.depth} aria-expanded={row.branch?row.expanded:undefined} aria-selected={active===n.id} tabIndex={active===n.id||!rows.some(r=>r.node.id===active)&&i===0?0:-1} data-tree-id={n.id} className={(active===n.id?'current ':'')+(row.match?'search-match':'')} key={n.id} style={{paddingLeft:8+Math.min(row.depth-1,20)*16}} onKeyDown={e=>key(e,row,i)}>
 {row.branch?<button className="tree-toggle" tabIndex={-1} aria-label={`${row.expanded?'Свернуть':'Развернуть'} ${n.name}`} onClick={()=>toggle(n.id)} disabled={!!query}>{row.expanded?'▾':'▸'}</button>:<span className="tree-leaf">·</span>}
 <button className="tree-name" tabIndex={-1} aria-label={row.branch?`Открыть папку ${n.name}`:`Выбрать компонент ${n.name}`} onClick={()=>select(row)} title={index.path(n.id).map(p=>p.name).join('/')}><strong><span className="tree-icon">{row.branch?'▣':'◇'}</span>{n.name}</strong><small>{n.type}</small></button>{row.branch&&<span className="tree-count">{index.children.get(n.id)?.length||0}</span>}
 </div>})}{rows.length===0&&<p className="muted">Компоненты не найдены.</p>}</div>
 </div>;
}
