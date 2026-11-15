# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Unit tests of a few kits on the built linux svm, without the rest of the profile.

    python3 tools/quicktest.py sproBase [sproControl ...]
"""
import subprocess
import sys

sys.path.insert(0, __import__('os').path.dirname(__file__))
from spro import home as H, linux as L
from spro.project import kits, closure

names = sys.argv[1:]
known = kits()
H.compile_kits(names)
out = H.BUILD / 'quick'
out.mkdir(exist_ok=True)
all_names = closure(['sys'] + names, known)
scode = H.scode(all_names, out / 'tests.xml', 8, test=True, test_kits=set(names))
svm = L.outdir('amd64') / 'run/svm'
r = subprocess.run([str(svm), str(scode), '-test'], cwd=out, capture_output=True, text=True)
text = r.stdout + r.stderr
print('\n'.join(l for l in text.splitlines() if 'FAIL' in l or 'Assert' in l or 'Test ' in l))
sys.exit(0 if r.returncode == 0 and 'Assert Failures  = 0' in text else 1)
