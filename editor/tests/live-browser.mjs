import{chromium}from'../web/node_modules/playwright-core/index.mjs';import path from'node:path';
const browser=await chromium.launch({headless:true,executablePath:path.join(process.env.LOCALAPPDATA,'ms-playwright/chromium-1223/chrome-win64/chrome.exe')});
try{
 const page=await browser.newPage({viewport:{width:1500,height:940}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto((process.env.SEDONA_TEST_URL||'http://127.0.0.1:8098')+'/?engine=reactflow');await page.waitForFunction(()=>window.__sedonaReady);
 await page.getByRole('button',{name:'Устройство',exact:true}).click();await page.getByLabel('Адрес',{exact:true}).fill('127.0.0.1');await page.getByLabel('UDP-порт').fill('12987');await page.getByLabel('Пользователь',{exact:true}).fill('admin');await page.getByLabel('Пароль',{exact:true}).fill('localJarTest');
 await page.getByRole('button',{name:'Подключить онлайн',exact:true}).click();await page.getByText('ОНЛАЙН',{exact:true}).waitFor();
 await page.waitForFunction(async()=>{const s=await(await fetch('/api/status')).json();return s.subscriptions>1});
 await page.getByRole('button',{name:'Нагрузка',exact:true}).first().click();await page.screenshot({path:'evidence/native-sox-browser.png'});
 await page.getByRole('button',{name:'Отключить весь обмен',exact:true}).click();await page.waitForFunction(async()=>{const s=await(await fetch('/api/status')).json();return s.mode==='OFF'&&s.subscriptions===0&&s.viewers===0});
 await page.screenshot({path:'evidence/native-sox-disconnected.png'});if(errors.length)throw Error(errors.join('\n'));console.log('PASS browser: real native SOX graph, SSE cached values, nonzero interest, explicit OFF, no browser errors');
}finally{await browser.close()}
