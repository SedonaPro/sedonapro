export function dataNumber(v){return v!==null&&v!==undefined&&typeof v!=='boolean'&&String(v).trim()!==''&&Number.isFinite(Number(v))?Number(v):null}
export function appendSample(history,sample,windowSeconds){return [...history.filter(p=>p.t>=sample.t-windowSeconds*1000),sample].slice(-600)}

export function csvCell(v){const text=String(v);return '"'+((typeof v!=='number'&&/^[=+\-@\t\r]/.test(text)?"'":"")+text).replaceAll('"','""')+'"'}
