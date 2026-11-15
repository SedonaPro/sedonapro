// Kit widget in the browser (run by kit_widget_checks.py): from the inspector, options, build with
// confirmation, operation face with live values, a setpoint written with confirmation.
import { chromium } from '../web/node_modules/playwright-core/index.mjs'; import path from 'node:path';
const out = process.env.SEDONA_TEST_OUT || '.';
const browser = await chromium.launch({ headless: true, executablePath: process.env.SEDONA_TEST_BROWSER });
let page; const errors = [];
try {
  page = await browser.newPage({ viewport: { width: 1500, height: 940 } });
  page.on('pageerror', e => errors.push(e.message));
  await page.goto(process.env.SEDONA_TEST_URL + '/?engine=reactflow'); await page.waitForFunction(() => window.__sedonaReady);
  await page.getByRole('button', { name: 'Устройство', exact: true }).click();
  await page.getByLabel('Адрес', { exact: true }).fill('127.0.0.1'); await page.getByLabel('UDP-порт').fill(process.env.SEDONA_TEST_SOX);
  await page.getByLabel('Пользователь', { exact: true }).fill('admin'); await page.getByLabel('Пароль', { exact: true }).fill('localJarTest');
  await page.getByRole('button', { name: 'Подключить онлайн', exact: true }).click(); await page.getByText('ОНЛАЙН', { exact: true }).waitFor();
  await page.getByText('unit', { exact: true }).first().click();
  await page.getByRole('button', { name: 'Сигналы', exact: true }).click();
  await page.getByRole('button', { name: 'Открыть виджет «unit»' }).click();
  const w = page.getByRole('dialog', { name: 'Виджет unit' });
  await w.getByText('Виджет с устройства (m:ui.zip)').waitFor();
  await w.getByRole('tab', { name: 'Работа' }).waitFor();
  await page.waitForFunction(() => [...document.querySelectorAll('.kit-widget svg text')].some(t => /°C/.test(t.textContent) && !/—/.test(t.textContent)));
  await page.screenshot({ path: path.join(out, 'web-operation.png') });
  await w.getByRole('tab', { name: 'Настройка' }).click();
  await w.getByLabel('Охладитель').selectOption({ label: 'водяной' });
  await w.getByText('Ещё не на устройстве: Охладитель').waitFor();
  await page.screenshot({ path: path.join(out, 'web-setup.png') });
  await w.getByRole('button', { name: 'Собрать…' }).click();
  const confirm = w.getByRole('alertdialog', { name: 'Подтверждение действия' });
  await confirm.getByText('Охладитель: нет → водяной').waitFor();
  await page.screenshot({ path: path.join(out, 'web-confirm.png') });
  await confirm.getByRole('button', { name: 'Подтвердить' }).click();
  await w.getByText(/Собрать: выполнено/).waitFor({ timeout: 30000 });
  await w.getByRole('tab', { name: 'Работа' }).click();
  await page.waitForFunction(() => document.querySelector('.kit-widget [data-element="cooler"]'));
  await w.getByLabel('Supply setpoint').or(w.getByLabel('Уставка притока')).fill('21.5');
  await w.getByRole('button', { name: 'Задать' }).first().click();
  await w.getByRole('alertdialog', { name: 'Подтверждение записи' }).getByRole('button', { name: 'Подтвердить' }).click();
  await w.getByText(/подтверждено чтением 21\.5/).waitFor();
  await page.screenshot({ path: path.join(out, 'web-operation-after.png') });
  await w.getByRole('button', { name: 'Закрыть' }).click();
  await page.getByRole('button', { name: 'Отключить весь обмен', exact: true }).click().catch(() => {});
  if (errors.length) throw Error(errors.join('\n'));
  console.log('PASS browser: widget from the inspector, live operation face, option chosen, build confirmed (cooler added), setpoint written with readback');
} catch (e) { if (page) await page.screenshot({ path: path.join(out, 'web-failed.png') }).catch(() => {}); throw e; } finally { await browser.close(); }
