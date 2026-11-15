import subprocess,json,urllib.request,urllib.error,time,zipfile,io
def check_layout(root,api,work):
 cp=str(root/'server/classes')+';'+str(root/'server/lib/sedona.jar')
 subprocess.run(['javac','-encoding','UTF-8','-cp',cp,'-d',str(root/'server/classes'),str(root/'tests/FixturePeer.java'),str(root/'tests/LayoutDisk.java')],check=True,capture_output=True)
 def peer(action='read-meta',value=None):
  cmd=['java','-Dsedona.home=F:/Sedona/sedonadev-master','-cp',cp,'FixturePeer',action]
  if value is not None:cmd.append(str(value))
  p=subprocess.run(cmd,check=True,capture_output=True,text=True);return int(next(x.split('=',1)[1] for x in p.stdout.splitlines() if x.startswith('PEER_VALUE=')))
 api('disconnect',{});peer('write-meta',0x1234)
 config=dict(host='127.0.0.1',port=12987,username='admin',password='localJarTest',mode='LIVE');j=api('connect',config);a=j['consumer'];target=next(n['id'] for n in j['graph']['nodes'] if n['name']=='probe');api('watch',dict(consumer=a,ids=[target]))
 changes=[dict(id=target,x=12,y=34,expectedMeta=0x1234)]
 result=api('layout/save',dict(consumer=a,changes=changes));assert result['complete'],result;expected=(12<<24)|(34<<16)|0x1234;assert peer()==expected,'Native read differs'
 time.sleep(.2);p=subprocess.run(['java','-Dsedona.home=F:/Sedona/sedonadev-master','-cp',cp,'LayoutDisk',str(work/'app.sab')],check=True,capture_output=True,text=True);assert 'DISK_META='+str(expected) in p.stdout,p.stdout
 conflict=api('layout/save',dict(consumer=a,changes=changes));assert not conflict['complete'] and peer()==expected,conflict
 catalog=api('catalog');assert any(t['type']=='types::ConstFloat' for t in catalog);assert any(p['name']=='out' for t in catalog if t['type']=='types::ConstFloat' for p in t['ports'])
 payload=urllib.request.urlopen('http://127.0.0.1:8098/api/kits/export').read();assert len(zipfile.ZipFile(io.BytesIO(payload)).namelist())==5
 req=urllib.request.Request('http://127.0.0.1:8098/api/kits/import',data=payload,headers={'Content-Type':'application/octet-stream'});uploaded=json.load(urllib.request.urlopen(req));assert len(uploaded['imported'])==5
 req=urllib.request.Request('http://127.0.0.1:8098/api/kits/import',data=b'<!DOCTYPE x><kitManifest/>',headers={'Content-Type':'application/octet-stream'})
 try:urllib.request.urlopen(req);raise AssertionError('DTD accepted')
 except urllib.error.HTTPError:pass
 api('disconnect',{});peer('write-meta',1)
 subprocess.run(['node',str(root/'tests/layout-browser.mjs')],cwd=root,check=True,timeout=75)
 (root/'evidence/layout-kits.json').write_text(json.dumps(dict(result='PASS',tests=['native meta readback','lower 16 flags preserved','App.save persists verified by independent SAB decoder','stale coordinates rejected','catalog contains real slots','manifest ZIP download/import','DTD rejected'],layoutReceipt=result),indent=2),encoding='utf-8')
