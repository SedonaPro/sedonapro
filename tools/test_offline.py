#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run built Linux test images without legacy external-network or serial tests."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=root/'build/linux-amd64-pro-sim')
    args = parser.parse_args()
    output = args.output.resolve()
    images = sorted(output.glob('tests-*.xml'))
    if not images:
        parser.error('No built test images; run ./spro build linux first')
    home = root/'build/home'
    compiler = ['java', '-Dsedona.home='+str(home), '-cp',
                os.pathsep.join(str(home/'lib'/name) for name in
                                ['sedona.jar', 'sedonac.jar']), 'sedonac.Main']
    assertions = 0
    for image in images:
        tree = ET.parse(image)
        for dependency in tree.getroot().findall('depend'):
            if dependency.get('on', '').split()[0] in {'inet', 'serial'}:
                dependency.set('test', 'false')
        offline = output/('offline-'+image.name)
        tree.write(offline)
        subprocess.run(compiler+[str(offline)], check=True)
        result = subprocess.run(
            [str(output/'run/svm'), str(offline.with_suffix('.scode')), '-test'],
            cwd=output, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, timeout=600)
        print(result.stdout, flush=True)
        failures = re.findall(r'Assert Failures\s*=\s*(\d+)', result.stdout)
        counts = re.findall(r'Assert Successes\s*=\s*(\d+)', result.stdout)
        if (result.returncode or not failures or any(int(n) for n in failures)
                or not counts or 'WARNING: stack imbalance' in result.stdout):
            raise SystemExit('FAIL: '+image.name)
        assertions += sum(int(n) for n in counts)
    print(f'OFFLINE PASS: {len(images)} images, {assertions} assertions')


if __name__ == '__main__':
    main()
