from pathlib import Path
import json,datetime,zipfile,hashlib
r=Path('.');version=json.loads((r/'web/package.json').read_text(encoding='utf-8'))['version'];out=r/'releases'/version;out.mkdir(parents=True,exist_ok=True)
archive=out/('mcs-sedona-web-lab-'+version+'.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
 for base in ['server/src','server/classes','server/lib','web/src','web/dist','tests','tools','evidence','examples']:
  for p in (r/base).rglob('*'):
   if not p.is_file() or p.name=='server.pid' or p.name.startswith(('LeaseTest','ManifestNetworkTest','FixturePeer','ExportCatalog','ProfileSecretsTest','LayoutDisk','LinkPeer','DuplicatePeer')) and p.suffix=='.class':continue
   z.write(p,p)
 for name in ['README_RU.md','Start.ps1','Stop.ps1','Start.cmd','Start-With-Writes.cmd','web/package.json','web/package-lock.json','web/index.html']:
  z.write(r/name,name)
(out/'release.json').write_text(json.dumps({'version':version,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'created':datetime.datetime.now().isoformat(),'runtime':'Java 17; no Node needed to run','deviceMode':'OFF by default; writes disabled unless explicitly enabled at startup'},indent=2),encoding='utf-8')
print(out)
