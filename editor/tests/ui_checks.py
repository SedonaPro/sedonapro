import json,uuid,subprocess,urllib.error,threading
def check_ui(root,api,work):
 api('disconnect',{})
 doc=dict(format='mcs-sedona-ui',version=1,id='ui-test-'+str(uuid.uuid4()),title='Fixture SVG',views=[dict(id='ahu1',title='Air handler',folder='app/service',svg='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 400"><rect x="40" y="80" width="720" height="240" rx="20" fill="#d6ede3"/><circle cx="250" cy="200" r="80" fill="#267d68"/><text x="400" y="210" font-size="32">MCS AHU</text></svg>')])
 try:
  saved=api('ui/save',dict(document=doc));assert saved['saved'];assert api('ui/load',dict(id=doc['id']))==doc
  for bad in [dict(doc,id='../app'),dict(doc,password='never-accepted'),dict(doc,views=doc['views']*2)]:
   try:api('ui/save',dict(document=bad));raise AssertionError('Invalid UI package accepted')
   except urllib.error.HTTPError:pass
  j=api('connect',dict(host='127.0.0.1',port=12987,username='admin',password='localJarTest',mode='LIVE'));a=j['consumer'];stop=threading.Event()
  def keep():
   while not stop.wait(1):
    try:api('watch',dict(consumer=a,ids=[]))
    except Exception:return
  thread=threading.Thread(target=keep,daemon=True);thread.start()
  try:
   app_before=(work/'app.sab').read_bytes();scode_before=(work/'kits.scode').read_bytes();receipt=api('ui/device/save',dict(consumer=a,document=doc));assert receipt['verified'];assert json.loads((work/'mcs-ui.json').read_text(encoding='utf-8'))==doc
   assert api('ui/device/load',dict(consumer=a))==doc
   assert app_before==(work/'app.sab').read_bytes();assert scode_before==(work/'kits.scode').read_bytes()
  finally:stop.set();thread.join(2);api('disconnect',{})
  try:api('ui/device/save',dict(consumer=a,document=doc));raise AssertionError('OFF write accepted')
  except urllib.error.HTTPError:pass
  subprocess.run(['node',str(root/'tests/ui-browser.mjs')],cwd=root,check=True,timeout=90)
  (root/'evidence/ui-storage.json').write_text(json.dumps(dict(result='PASS',tests=['local JSON roundtrip','unsafe ID/unknown field/duplicate view rejected','native SOX file upload and readback','independent device filesystem verification','app.sab and kits.scode unchanged','OFF write denied','browser SVG attachment/fullscreen/export/project roundtrip/script rejection']),indent=2),encoding='utf-8')
 finally:
  (root/'data/ui-projects'/f"{doc['id']}.json").unlink(missing_ok=True)
  for p in (root/'data/ui-projects/backups').glob(doc['id']+'-*.json'):p.unlink()
