import urllib.request,threading,time,json,subprocess
def check_sse(root,api):
 api('disconnect',{});j=api('connect',dict(host='127.0.0.1',port=12987,username='admin',password='localJarTest',mode='LIVE'));consumer=j['consumer'];api('watch',dict(consumer=consumer,ids=[]));stop=threading.Event();opened=threading.Event();errors=[]
 def stream():
  try:
   with urllib.request.urlopen('http://127.0.0.1:8098/api/events?consumer='+consumer,timeout=20) as response:
    opened.set()
    while not stop.is_set():
     if not response.readline():break
  except Exception as e:errors.append(type(e).__name__)
 thread=threading.Thread(target=stream,daemon=True);thread.start();assert opened.wait(4);time.sleep(11);assert api('status')['mode']=='LIVE','Active SSE died without JS timer heartbeat';stop.set();thread.join(4);time.sleep(11);s=api('status');assert s['mode']=='OFF',s;assert 'heartbeat expired' in s['lastError'],s;assert not errors,errors
 subprocess.run(['node',str(root/'tests/sse-browser.mjs')],cwd=root,check=True,timeout=70)
 (root/'evidence/sse-recovery.json').write_text(json.dumps(dict(result='PASS',tests=['working SSE keeps viewer alive without JS heartbeat timer','closing stream without heartbeat expires session','expiry reason preserved','browser SSE EOF recovers same SOX session']),indent=2),encoding='utf-8')
