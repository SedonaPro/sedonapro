import {chromium} from '../web/node_modules/playwright-core/index.mjs';import fs from 'node:fs';import path from 'node:path';
const dir=path.resolve('evidence');const browser=await chromium.launch({headless:true,executablePath:process.env.SEDONA_TEST_CHROME||path.join(process.env.LOCALAPPDATA,'ms-playwright/chromium-1223/chrome-win64/chrome.exe')});
const results=[];
try{
 for(const engine of ['x6','reactflow'])for(const count of [100,500,1000]){
  const page=await browser.newPage({viewport:{width:1500,height:940}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const start=performance.now();await page.goto(`http://127.0.0.1:8098/?engine=${engine}&count=${count}`,{waitUntil:'networkidle'});await page.waitForFunction(()=>window.__sedonaReady,{timeout:90000});
  const firstMs=performance.now()-start;const info=await page.evaluate(()=>({...window.__benchmarkInfo,mountMs:window.__mountMs,listeners:window.__valueListeners()}));
  const expected=engine==='x6'?'.x6-node':'.react-flow__node';const rendered=await page.locator(expected).count();
  if(rendered!==count)throw Error(`Incomplete render ${engine}: ${rendered}/${count}`);
  const updates=await page.evaluate(async()=>{const times=[];for(let i=0;i<12;i++){const start=performance.now();window.__tick();await new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)));times.push(performance.now()-start);}return times;});
  await page.getByRole('button',{name:'Нагрузка',exact:true}).first().click();await page.screenshot({path:path.join(dir,`${engine}-${count}.png`)});
  if(errors.length)throw Error(errors.join('\n'));
  results.push({engine,count,rendered,...info,navigationMs:firstMs,updateDoubleRafMs:updates});console.log(JSON.stringify(results.at(-1)));
  // Both engines must clean up listeners when switched; no old 1000-node subscriptions remain.
  if(count===1000){await page.getByRole('button',{name:engine==='x6'?'React Flow':'AntV X6',exact:true}).click();await page.waitForFunction(()=>window.__sedonaReady);await page.waitForTimeout(300);const listeners=await page.evaluate(()=>window.__valueListeners());if(listeners>1001)throw Error('Listener leak after engine switch: '+listeners);}
  await page.close();
 }
 fs.writeFileSync(path.join(dir,'browser-benchmark.json'),JSON.stringify({method:'Single headless Chromium run per engine/count. All nodes mounted; 8 fields per node, 100 updated nodes per tick; simple orthogonal/smoothstep links, no obstacle router. double RAF includes frame scheduling, not isolated rendering CPU.',results},null,2));
}finally{await browser.close()}
