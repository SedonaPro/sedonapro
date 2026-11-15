from pathlib import Path
import os
import sys,subprocess,socket,threading,time,json,urllib.request,urllib.error,xml.etree.ElementTree as E
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent/'bcSedona/tests'));import local_fixture
old,work=local_fixture.prepare(r)
for testport in [12986,12987]:
 check=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
 try:check.bind(('127.0.0.1',testport))
 finally:check.close()
tree=E.parse(work/'app.sax');tree.getroot().find(".//comp[@name='sox']/prop[@name='port']").set('val','12986');E.SubElement(tree.getroot().find('app'),'comp',name='probe',type='types::ConstFloat')
tree.write(work/'app.sax',encoding='utf-8')
if os.environ.get('SEDONA_WIDGET_TEST')=='1' or os.environ.get('SEDONA_SVG_TEST')=='1':
 E.SubElement(tree.getroot().find('app'),'comp',name='bool1',type='types::ConstBool');tree.write(work/'app.sax',encoding='utf-8')
if os.environ.get('SEDONA_SVG_TEST')=='1':
 E.SubElement(tree.getroot().find('app'),'comp',name='alarm',type='types::ConstBool');tree.write(work/'app.sax',encoding='utf-8')
subprocess.run(['java','-Dsedona.home='+str(old),'-cp',str(old/'lib/sedona.jar')+';'+str(old/'lib/sedonac.jar'),'sedonac.Main',str(work/'app.sax')],check=True,capture_output=True)
cp=str(r/'server/classes')+';'+str(r/'server/lib/sedona.jar')
subprocess.run(['javac','-encoding','UTF-8','-cp',cp,'-d',str(r/'server/classes'),str(r.parent/'ontrol-mcs-port/tests/ManifestNetworkTest.java')],check=True,capture_output=True)
subprocess.run(['java','-Dsedona.home='+str(old),'-cp',cp,'ManifestNetworkTest','prepare',str(work)],check=True,capture_output=True)
log=open(work/'svm-integration.log','wb');proc=subprocess.Popen([str(old/'bin/svm.exe'),'kits.scode','app.sab'],cwd=work,stdout=log,stderr=subprocess.STDOUT,creationflags=0x08000000)
relay=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);relay.bind(('127.0.0.1',12987));relay.settimeout(.2);stop=threading.Event();packets=[0,0]
def proxy():
 client=None
 while not stop.is_set():
  try:data,addr=relay.recvfrom(65535)
  except socket.timeout:continue
  if addr[1]==12986:
   packets[1]+=1
   if client:relay.sendto(data,client)
  else:
   packets[0]+=1;client=addr;relay.sendto(data,('127.0.0.1',12986))
thread=threading.Thread(target=proxy,daemon=True);thread.start()
base=os.environ.get('SEDONA_TEST_URL','http://127.0.0.1:8098')+'/api/'
def api(path,body=None):
 req=urllib.request.Request(base+path,data=None if body is None else json.dumps(body).encode(),headers={} if body is None else {'Content-Type':'application/json'})
 return json.load(urllib.request.urlopen(req,timeout=45))
report={}
try:
 api('disconnect',{});time.sleep(.4);before=packets[:];time.sleep(1);assert packets==before,'OFF sent UDP traffic'
 # Only public local fixture credentials are used; never persisted in a test report.
 config=dict(host='127.0.0.1',port=12987,username='admin',password='localJarTest',mode='LIVE')
 first=api('connect',config);a=first['consumer'];assert any(n['name']=='probe' for n in first['graph']['nodes'])
 assert 'cred' not in json.dumps(first['graph']['values']),'Credentials leaked into graph'
 target=next(n['id'] for n in first['graph']['nodes'] if n['name']=='probe')
 s=api('watch',dict(consumer=a,ids=[target]));assert s['subscriptions']==1,s
 b=api('attach',{})['consumer'];s=api('watch',dict(consumer=b,ids=[target]));assert s['subscriptions']==1 and s['viewers']==2,s
 adds=s['subscriptionAdds'];s=api('release',dict(consumer=a));assert s['subscriptions']==1 and s['viewers']==1 and s['subscriptionAdds']==adds,s
 # An empty interest set really sends unsubscribe while the session remains open.
 s=api('watch',dict(consumer=b,ids=[]));assert s['subscriptions']==0 and s['subscriptionRemoves']>=1,s
 s=api('watch',dict(consumer=b,ids=[target]));assert s['subscriptions']==1
 api('disconnect',{});time.sleep(.5);closed=packets[:];time.sleep(2);assert packets==closed,'UDP continued after explicit OFF'
 # A dead browser must not keep the device subscribed forever.
 a=api('connect',config)['consumer'];api('watch',dict(consumer=a,ids=[target]));time.sleep(10)
 s=api('status');assert s['mode']=='OFF' and s['subscriptions']==0 and s['viewers']==0,s
 time.sleep(.5);expired=packets[:];time.sleep(1);assert packets==expired,'Reconnect after lease expiry'
 # Snapshot reads once and closes its own device session.
 config['mode']='SNAPSHOT';snapshot=api('connect',config);assert snapshot['status']['mode']=='SNAPSHOT' and snapshot['status']['viewers']==0
 time.sleep(.5);frozen=packets[:];time.sleep(1);assert packets==frozen,'Snapshot retained UDP session'
 try:
  req=urllib.request.Request(base+'disconnect',data=b'{}',headers={'Origin':'http://untrusted.invalid','Content-Type':'application/json'});urllib.request.urlopen(req);raise AssertionError('Cross-origin POST accepted')
 except urllib.error.HTTPError as e:assert e.code==400
 subprocess.run(['node',str(r/'tests/live-browser.mjs')],cwd=r,check=True,timeout=60)
 if os.environ.get('SEDONA_SVG_TEST')=='1':
  subprocess.run(['javac','-encoding','UTF-8','-cp',cp,'-d',str(r/'server/classes'),str(r/'tests/FixturePeer.java')],check=True,capture_output=True)
  subprocess.run(['node',str(r/'tests/svg-library-browser.mjs')],cwd=r,check=True,timeout=180)
 if os.environ.get('SEDONA_WIDGET_TEST')=='1':
  subprocess.run(['javac','-encoding','UTF-8','-cp',cp,'-d',str(r/'server/classes'),str(r/'tests/FixturePeer.java')],check=True,capture_output=True)
  subprocess.run(['node',str(r/'tests/widgets-browser.mjs')],cwd=r,check=True,timeout=150)
 if os.environ.get('SEDONA_UI_TEST')=='1':
  from ui_checks import check_ui
  check_ui(r,api,work)
 if os.environ.get('SEDONA_WRITE_TEST')=='1':
  from write_checks import check_writes
  slot=next(p['id'] for n in first['graph']['nodes'] if n['name']=='probe' for p in n['ports'] if p['name']=='out')
  check_writes(r,api,target,slot)
 if os.environ.get('SEDONA_LAYOUT_TEST')=='1':
  from layout_checks import check_layout
  check_layout(r,api,work)
 if os.environ.get('SEDONA_CREATE_TEST')=='1':
  from create_checks import check_create
  check_create(r,api,work)
 if os.environ.get('SEDONA_PROFILE_TEST')=='1':
  from profile_checks import check_profile
  check_profile(r,api)
 if os.environ.get('SEDONA_LINK_TEST')=='1':
  from link_checks import check_links
  check_links(r,api,work)
 if os.environ.get('SEDONA_SSE_TEST')=='1':
  from sse_checks import check_sse
  check_sse(r,api)
 if os.environ.get('SEDONA_DUPLICATE_TEST')=='1':
  from duplicate_checks import check_duplicate
  check_duplicate(r,api,work)
 time.sleep(.5);browser_off=packets[:];time.sleep(1);assert packets==browser_off,'Browser OFF did not stop traffic'
 report=dict(result='PASS',fixture='Native local Sedona SVM, 5 kits + types::ConstFloat, UDP relay counts independently',tests=['OFF has zero UDP','cold-cache manifest recovery','no credential slot exposure','shared subscription deduplication','partial release retains needed subscription','empty interest unsubscribes','explicit OFF stops UDP','dead browser lease closes session','no automatic reconnect','snapshot closes transport','cross-origin mutation rejected','real browser connects, renders native graph and turns traffic OFF'],packets=packets,finalStatus=api('status'))
 print(json.dumps(report,indent=2))
finally:
 try:api('disconnect',{})
 except Exception:pass
 stop.set();thread.join(1);relay.close();proc.terminate();proc.wait(timeout=10);log.close()
 if report:(r/'evidence/integration.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

