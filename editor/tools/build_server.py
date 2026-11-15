from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1]
# The Sedona library: a copy in server/lib, else the one SedonaPro builds (./spro build linux).
jar=r/'server/lib/sedona.jar'
if not jar.is_file():jar=r.parent/'build/home/lib/sedona.jar'
if not jar.is_file():raise SystemExit('sedona.jar missing: run ./spro build linux first')
(r/'server/classes').mkdir(exist_ok=True)
subprocess.run(['javac','--release','17','-encoding','UTF-8','-cp',str(jar),'-d',str(r/'server/classes'),*map(str,(r/'server/src').glob('*.java'))],check=True)
print('server classes built against',jar)
