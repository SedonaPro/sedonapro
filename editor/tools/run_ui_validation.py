"""Isolated OFF gateway for UI tests; never touches the user's device session."""
from pathlib import Path
import subprocess,time,urllib.request,shutil,sys
r=Path(__file__).resolve().parents[1];work=r/'build/widget-validation';shutil.copytree(r/'web/dist',work/'web/dist',dirs_exist_ok=True)
p=subprocess.Popen(['java','-Dport=8102','-Dsedona.web.root='+str(r),'-Dsedona.home='+str(r/'data'),'-cp',str(r/'server/classes')+';'+str(r/'server/lib/sedona.jar'),'WebServer'],cwd=work,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=0x08000000)
try:
 for i in range(30):
  if p.poll() is not None:raise RuntimeError('Test gateway could not start')
  try:urllib.request.urlopen('http://127.0.0.1:8102/api/status',timeout=1);break
  except Exception:time.sleep(.2)
 subprocess.run(['node',sys.argv[1]],cwd=r,check=True)
finally:p.terminate();p.wait(timeout=10)
