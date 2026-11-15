import subprocess,threading,time,json,urllib.error
def check_writes(root,api,target,slot):
 def peer(action='read',value=''):
  cmd=['java','-Dsedona.home=F:/Sedona/sedonadev-master','-cp',str(root/'server/classes')+';'+str(root/'server/lib/sedona.jar'),'FixturePeer',action]
  if value:cmd.append(value)
  r=subprocess.run(cmd,check=True,capture_output=True,text=True,timeout=15)
  return next(x.split('=',1)[1] for x in r.stdout.splitlines() if x.startswith('PEER_VALUE='))
 def rejected(path,body,needle):
  try:api(path,body);raise AssertionError('Unexpected write acceptance')
  except urllib.error.HTTPError as e:
   message=json.load(e)['error'];assert needle in message,message
 config=dict(host='127.0.0.1',port=12987,username='admin',password='localJarTest',mode='LIVE')
 assert api('status')['writesEnabled'];peer('write','0');a=api('connect',config)['consumer'];api('watch',dict(consumer=a,ids=[target]));stop=threading.Event()
 def heartbeat():
  while not stop.wait(1):
   try:api('watch',dict(consumer=a,ids=[target]))
   except Exception:return
 thread=threading.Thread(target=heartbeat,daemon=True);thread.start()
 def prepare(value):return api('write/prepare',dict(consumer=a,componentId=target,slotId=slot,value=value))
 try:
  p=prepare('12.5');assert float(peer())==0,'Preview wrote a value'
  b=api('attach',{})['consumer'];rejected('write/prepare',dict(consumer=b,componentId=target,slotId=slot,value='1'),'Another viewer');rejected('write/commit',dict(consumer=b,token=p['token']),'belongs');api('release',dict(consumer=b))
  api('write/cancel',dict(consumer=a,token=p['token']));rejected('write/commit',dict(consumer=a,token=p['token']),'missing')
  p=prepare('12.5');peer('write','99');rejected('write/commit',dict(consumer=a,token=p['token']),'changed since preview');assert float(peer())==99,'Conflict overwrote independent peer'
  p=prepare('42');result=api('write/commit',dict(consumer=a,token=p['token']));assert result['status']=='VERIFIED' and float(result['readBack'])==42 and not result['persistedToApp'];assert float(peer())==42
  rejected('write/commit',dict(consumer=a,token=p['token']),'missing');rejected('write/prepare',dict(consumer=a,componentId=target,slotId=slot,value='NaN'),'finite');rejected('write/prepare',dict(consumer=a,componentId=target,slotId=0,value='3'),'read only')
  p=prepare('17');api('disconnect',{});new=api('connect',config)['consumer'];rejected('write/commit',dict(consumer=new,token=p['token']),'missing');api('disconnect',{})
 finally:
  stop.set();thread.join(2);api('disconnect',{});peer('write','0')
 subprocess.run(['node',str(root/'tests/write-browser.mjs')],cwd=root,check=True,timeout=60)
 assert float(peer())==22.75,'Browser receipt differs from independent native read'
 peer('write','0')
 report=dict(result='PASS',target='Local test SVM only',tests=['prepare performs no write','single pending writer','wrong viewer rejected','cancel revokes token','independent peer conflict blocks commit','readback equals independent peer','token cannot be replayed','NaN rejected','readonly rejected','OFF/reconnect invalidates plans','browser explicit confirmation','UI verified value independently reread','test value restored'])
 (root/'evidence/property-write.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('PASS scalar write: conflict/readback/replay/lease/readonly checks')
