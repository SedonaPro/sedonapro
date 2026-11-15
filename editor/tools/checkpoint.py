from pathlib import Path
import zipfile,hashlib,datetime,sys,json
r=Path(__file__).resolve().parents[1];label=sys.argv[1] if len(sys.argv)>1 else 'manual'
if not label.replace('-','').isalnum():raise ValueError('Invalid checkpoint label')
out=r/'checkpoints'/(datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+label);out.mkdir(parents=True)
with zipfile.ZipFile(out/'source.zip','w',zipfile.ZIP_DEFLATED) as z:
 for base in ['server/src','web/src','tests','tools','evidence','examples','data/manifests','data/kit-imports','data/ui-projects']:
  for p in (r/base).rglob('*'):
   if p.is_file() and p.suffix in ['.java','.js','.jsx','.css','.mjs','.py','.ps1','.json','.md','.png','.log','.xml','.bin','.svg'] and p.name!='server.pid':z.write(p,p.relative_to(r))
 for n in ['web/package.json','web/package-lock.json','web/index.html','README_RU.md','Start.ps1','Stop.ps1','Start.cmd','Start-With-Writes.cmd','data/catalog-parts.json']:
  p=r/n
  if p.exists():z.write(p,n)
(out/'sha256.json').write_text(json.dumps({'source.zip':hashlib.sha256((out/'source.zip').read_bytes()).hexdigest()},indent=2))
print(out)
