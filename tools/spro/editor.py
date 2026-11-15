"""./spro editor build|run|test - the web editor (editor/: Java server + React).

Needs Node.js 20.19+ for the browser part (SPRO_NODE=/path/to/node, else `node` on PATH) and a
JDK 17+ for the server (JAVA_HOME, else `javac`/`java` on PATH). The SDK (sedona.jar, manifests,
widgets) comes from ./spro build linux.
"""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from . import home as H
from . import project as P

EDITOR = P.ROOT / 'editor'


def _version(cmd, pattern):
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    except OSError:
        return None
    m = re.search(pattern, out.stdout + out.stderr)
    return tuple(int(x) for x in m.groups()) if m else None


def node():
    for cand in (os.environ.get('SPRO_NODE'), shutil.which('node')):
        if cand and (_version([cand, '--version'], r'v(\d+)\.(\d+)') or (0, 0)) >= (20, 19):
            return cand
    sys.exit('editor: Node.js 20.19 or newer is needed (set SPRO_NODE=/path/to/bin/node)')


def jdk_tool(name):
    for base in (os.environ.get('JAVA_HOME'),):
        if base and (Path(base) / 'bin' / name).exists():
            cand = str(Path(base) / 'bin' / name)
            break
    else:
        cand = shutil.which(name)
    if not cand or (_version([cand, '-version'], r'(?:javac |version "?)(\d+)') or (0,)) < (17,):
        sys.exit('editor: a JDK 17 or newer is needed for the server (set JAVA_HOME)')
    return cand


def sedona_jar():
    jar = H.HOME / 'lib/sedona.jar'
    if not jar.is_file():
        sys.exit('editor: run ./spro build linux first (it provides sedona.jar and the manifests)')
    return jar


def build():
    n = node()
    npm = [n, str(Path(n).resolve().parent.parent / 'lib/node_modules/npm/bin/npm-cli.js')]
    if not Path(npm[1]).is_file():
        npm = [shutil.which('npm') or sys.exit('editor: npm not found next to node')]
    env = dict(os.environ, PATH=str(Path(n).parent) + os.pathsep + os.environ.get('PATH', ''))
    web = EDITOR / 'web'
    lock, modules = web / 'package-lock.json', web / 'node_modules/.package-lock.json'
    if not modules.is_file() or modules.stat().st_mtime < lock.stat().st_mtime:
        print('+ npm ci (editor/web)', flush=True)
        subprocess.run(npm + ['ci', '--no-audit', '--no-fund'], cwd=web, env=env, check=True)
    subprocess.run(npm + ['run', 'build'], cwd=web, env=env, check=True)
    javac = jdk_tool('javac')
    classes = EDITOR / 'server/classes'
    classes.mkdir(parents=True, exist_ok=True)
    subprocess.run([javac, '--release', '17', '-encoding', 'UTF-8', '-cp', str(sedona_jar()), '-d', str(classes),
                    *map(str, sorted((EDITOR / 'server/src').glob('*.java')))], check=True)
    print('BUILT editor: web/dist + server/classes')
    return 0


def run(port, writes):
    if not (EDITOR / 'server/classes/WebServer.class').is_file() or not (EDITOR / 'web/dist/index.html').is_file():
        sys.exit('editor: ./spro editor build first')
    java = jdk_tool('java')
    cmd = [java, '-Dsedona.home=' + str(H.HOME), '-Dport=%d' % port, '-cp',
           os.pathsep.join([str(EDITOR / 'server/classes'), str(sedona_jar())]), 'WebServer']
    if writes:
        cmd.insert(1, '-Dsedona.allowWrites=true')
    print('editor: http://127.0.0.1:%d (loopback only)%s' % (port, '; device writes ENABLED' if writes else ''), flush=True)
    os.chdir(EDITOR)
    os.execv(java, cmd)


def test():
    n = node()
    rc = subprocess.run([n, '--test', *map(str, sorted((EDITOR / 'tests').glob('*.test.mjs')))]).returncode
    env = dict(os.environ, SEDONA_TEST_NODE=n)
    java = jdk_tool('java')
    env['PATH'] = str(Path(java).parent) + os.pathsep + env.get('PATH', '')
    rc2 = subprocess.run([sys.executable, str(EDITOR / 'tests/kit_widget_checks.py')], env=env).returncode
    return rc or rc2
