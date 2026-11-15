import React,{useState,useEffect,useRef}from'react';import{canInput,scalar}from'./project';
export default function WritePanel({node,consumer,status,api}){
 const[field,setField]=useState(''),[value,setValue]=useState(''),[plan,setPlan]=useState(null),[receipt,setReceipt]=useState(null),[error,setError]=useState(''),[busy,setBusy]=useState(false);
 const planRef=useRef(plan);planRef.current=plan;
 function cancel(){const p=planRef.current;if(p)api('write/cancel',{consumer,token:p.token}).catch(()=>{});setPlan(null);}
 useEffect(()=>()=>{const p=planRef.current;if(p)api('write/cancel',{consumer,token:p.token}).catch(()=>{})},[node?.id,consumer]);
 useEffect(()=>{setPlan(null);setReceipt(null);setError('');setField('');setValue('')},[node?.id,consumer]);
 if(!consumer)return <div className="note">Для записи одного параметра требуется активное подключение. Черновик проекта автоматически не применяется.</div>;
 if(!status.writesEnabled)return <div className="note">Запись выключена при запуске сервера. Для стендовой проверки сервис запускается через Start.ps1 -EnableWrites. Обычный запуск оставляет устройство только для чтения.</div>;
 if(!node)return <div className="note">Выберите компонент на схеме.</div>;
 const ports=node.ports.filter(p=>canInput(p)&&['bool','byte','short','int','long','float','double'].includes(scalar(p.type)));
 async function prepare(){setBusy(true);setError('');setReceipt(null);setPlan(null);try{setPlan(await api('write/prepare',{consumer,componentId:+node.id.slice(1),slotId:+field,value}));}catch(e){setError(e.message)}finally{setBusy(false)}}
 async function apply(){const token=plan.token;setPlan(null);setBusy(true);setError('');try{setReceipt(await api('write/commit',{consumer,token}))}catch(e){setError(e.message+' Подтверждение не повторяется автоматически. Перечитайте значение перед новой попыткой.')}finally{setBusy(false)}}
 return <div className="write-panel"><h4>Запись параметра устройства</h4><p className="muted">Отдельная операция с readback; не загрузка проекта и не сохранение app.sab.</p><label>Параметр устройства<select aria-label="Параметр устройства" value={field} onChange={e=>{setField(e.target.value);cancel()}}><option value="">Выберите поле</option>{ports.map(p=><option key={p.id} value={p.id}>{p.name} · {scalar(p.type)}</option>)}</select></label><label>Новое значение устройства<input aria-label="Новое значение устройства" value={value} onChange={e=>{setValue(e.target.value);cancel()}}/></label><button disabled={busy||!field} onClick={prepare}>Прочитать и подготовить</button>
 {plan&&<div className="write-preview"><strong>{plan.component}.{plan.slot}</strong><div>Было: <b>{plan.before}</b></div><div>Станет: <b>{plan.desired}</b></div><small>Подтверждение действует 30 секунд. Значение ещё не записано.</small><button className="danger" disabled={busy} onClick={apply}>Подтвердить запись параметра</button><button onClick={cancel}>Отменить подготовку</button></div>}
 {receipt&&<div className="note" role="status">{receipt.status==='VERIFIED'?'Значение подтверждено обратным чтением':'Обратное чтение не совпало'}: {receipt.readBack}. В app.sab не сохранялось.</div>}{error&&<div className="error" role="alert">{error}</div>}
 </div>;
}
