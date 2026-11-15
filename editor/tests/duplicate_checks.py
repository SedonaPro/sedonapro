import subprocess,json,uuid,threading,urllib.error
def check_duplicate(root,api,work):
 cp=str(root/'server/classes')+';'+str(root/'server/lib/sedona.jar');subprocess.run(['javac','-encoding','UTF-8','-cp',cp,'-d',str(root/'server/classes'),str(root/'tests/DuplicatePeer.java')],check=True,capture_output=True)
 api('disconnect',{});j=api('connect',dict(host='127.0.0.1',port=12987,username='admin',password='localJarTest',mode='LIVE'));a=j['consumer'];stop=threading.Event()
 def keep():
  while not stop.wait(1):
   try:api('watch',dict(consumer=a,ids=[0]))
   except Exception:return
 thread=threading.Thread(target=keep,daemon=True);thread.start()
 try:
  part=next(p for p in j['graph']['origin']['parts'] if p.startswith('math-'));source=api('component/create',dict(consumer=a,type='math::Avg10',kitPart=part,parentId=0,x=10,y=10,token=str(uuid.uuid4())));n=next(n for n in source['graph']['nodes'] if n['id']==source['id']);slot=next(p['id'] for p in n['ports'] if p['name']=='maxTime');plan=api('write/prepare',dict(consumer=a,componentId=n['id'],slotId=slot,value='1234'));api('write/commit',dict(consumer=a,token=plan['token']))
  request=dict(consumer=a,sourceId=n['id'],token=str(uuid.uuid4()));copy=api('component/duplicate',request);assert copy['created'] and copy['configVerified'] and copy['appSaveRequested'],copy;assert copy['id']!=n['id'];assert copy['graph']['values']['n'+str(copy['id'])]['maxTime']=='1234';assert len(copy['graph']['edges'])==0
  for args in [['live',copy['name']],['disk',str(work/'app.sab'),copy['name']]]:
   p=subprocess.run(['java','-Dsedona.home=F:/Sedona/sedonadev-master','-cp',cp,'DuplicatePeer',*args],check=True,capture_output=True,text=True);assert 'CONFIG=1234' in p.stdout,p.stdout
  try:api('component/duplicate',request);raise AssertionError('Duplicate replay accepted')
  except urllib.error.HTTPError:pass
 finally:stop.set();thread.join(2);api('disconnect',{})
 subprocess.run(['node',str(root/'tests/duplicate-browser.mjs')],cwd=root,check=True,timeout=60)
 (root/'evidence/duplicate-native.json').write_text(json.dumps(dict(result='PASS',tests=['copy config via fresh SOX reads','unique component ID/name','no external wires copied','independent native config readback','app.sab config persistence','same token replay rejected','online context menu and Ctrl+D']),indent=2),encoding='utf-8')
