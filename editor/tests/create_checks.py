import subprocess,json,uuid,urllib.error
def check_create(root,api,work):
 cp=str(root/'server/classes')+';'+str(root/'server/lib/sedona.jar');subprocess.run(['javac','-encoding','UTF-8','-cp',cp,'-d',str(root/'server/classes'),str(root/'tests/FixturePeer.java'),str(root/'tests/LayoutDisk.java')],check=True,capture_output=True)
 api('disconnect',{});j=api('connect',dict(host='127.0.0.1',port=12987,username='admin',password='localJarTest',mode='LIVE'));a=j['consumer'];api('watch',dict(consumer=a,ids=[0]));part=next(p for p in j['graph']['origin']['parts'] if p.startswith('types-'))
 request=dict(consumer=a,type='types::ConstFloat',kitPart=part,parentId=0,x=20,y=10,token=str(uuid.uuid4()))
 def rejected(body):
  try:api('component/create',body);raise AssertionError('Unexpected create acceptance')
  except urllib.error.HTTPError:pass
 rejected(dict(request,kitPart='types-00000000'));assert len(api('graph')['nodes'])==len(j['graph']['nodes'])
 result=api('component/create',request);assert result['created'] and result['appSaveRequested'],result;name=result['name'];assert len(result['graph']['nodes'])==len(j['graph']['nodes'])+1
 p=subprocess.run(['java','-Dsedona.home=F:/Sedona/sedonadev-master','-Dfixture.component='+name,'-cp',cp,'FixturePeer','read-meta'],check=True,capture_output=True,text=True);expected=(20<<24)|(10<<16)|1;assert 'PEER_VALUE='+str(expected) in p.stdout,p.stdout
 p=subprocess.run(['java','-Dsedona.home=F:/Sedona/sedonadev-master','-cp',cp,'LayoutDisk',str(work/'app.sab'),name],check=True,capture_output=True,text=True);assert 'DISK_META='+str(expected) in p.stdout,p.stdout
 api('watch',dict(consumer=a,ids=[0]));rejected(request);assert len(api('graph')['nodes'])==len(j['graph']['nodes'])+1
 api('disconnect',{});subprocess.run(['node',str(root/'tests/palette-browser.mjs'),'live'],cwd=root,check=True,timeout=60)
 (root/'evidence/create-component.json').write_text(json.dumps(dict(result='PASS',tests=['exact installed kit required','component created via SOX','native independent meta readback','created component verified in app.sab','same token replay rejected','live palette drag creates component']),indent=2),encoding='utf-8')
