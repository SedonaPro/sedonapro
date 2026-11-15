import subprocess,json,uuid,urllib.error
def check_links(root,api,work):
 cp=str(root/'server/classes')+';'+str(root/'server/lib/sedona.jar');subprocess.run(['javac','-encoding','UTF-8','-cp',cp,'-d',str(root/'server/classes'),str(root/'tests/LinkPeer.java')],check=True,capture_output=True)
 api('disconnect',{});j=api('connect',dict(host='127.0.0.1',port=12987,username='admin',password='localJarTest',mode='LIVE'));a=j['consumer'];api('watch',dict(consumer=a,ids=[0]));src=next(n for n in j['graph']['nodes'] if n['name']=='probe');part=next(p for p in j['graph']['origin']['parts'] if p.startswith('math-'))
 created=api('component/create',dict(consumer=a,type='math::Add2',kitPart=part,parentId=0,x=30,y=5,token=str(uuid.uuid4())));dst=next(n for n in created['graph']['nodes'] if n['id']==created['id']);out=next(p['id'] for p in src['ports'] if p['name']=='out');inp=next(p['id'] for p in dst['ports'] if p['name']=='in1')
 request=dict(consumer=a,source=src['id'],sourceSlot=out,target=dst['id'],targetSlot=inp,remove=False,token=str(uuid.uuid4()))
 def peer(mode):
  args=['disk',str(work/'app.sab')] if mode=='disk' else ['live',created['name']];p=subprocess.run(['java','-Dsedona.home=F:/Sedona/sedonadev-master','-cp',cp,'LinkPeer',*args],check=True,capture_output=True,text=True);return p.stdout
 linked=api('link/change',request);assert linked['verified'] and linked['appSaveRequested'],linked;assert 'PEER_LINKS=1' in peer('live');assert 'DISK_LINKS=1' in peer('disk');api('watch',dict(consumer=a,ids=[0]))
 try:api('link/change',request);raise AssertionError('Replay accepted')
 except urllib.error.HTTPError:pass
 removed=api('link/change',dict(request,remove=True,token=str(uuid.uuid4())));assert removed['verified'];assert 'PEER_LINKS=0' in peer('live');assert 'DISK_LINKS=0' in peer('disk');api('disconnect',{})
 subprocess.run(['node',str(root/'tests/wire-live-browser.mjs')],cwd=root,check=True,timeout=60)
 (root/'evidence/link-native.json').write_text(json.dumps(dict(result='PASS',tests=['SOX link and unlink','independent native readback','App.save link persistence verified by SAB decoder','same request token rejected','live browser drag and context unlink']),indent=2),encoding='utf-8')
