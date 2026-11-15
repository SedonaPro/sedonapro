# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Packages of the linux target: tar.gz with install.sh, .deb, Docker image.

build/packages/
  sedonapro-<ver>-linux-<arch>-<board>/      staged package directory
  sedonapro-<ver>-linux-<arch>-<board>.tar.gz
  sedonapro_<ver>_<arch>.deb                 (board in the package description)
  SHA256SUMS
"""
import hashlib
import os
import shutil
import subprocess
import tarfile

from . import home as H
from . import linux as L
from .project import ROOT, PROJECT, select

INSTALL = ROOT / 'targets/linux/install'
DOCKER = ROOT / 'targets/linux/docker'
OUT = H.BUILD / 'packages'
DEB_DEPENDS = {'-lmodbus': 'libmodbus5', '-lsqlite3': 'libsqlite3-0'}


def stage(arch, profile, board):
    profile = profile or PROJECT['targets']['linux']['default_profile']
    board = board or PROJECT['targets']['linux']['default_board']
    run = L.outdir(arch, profile, board) / 'run'
    if not (run / 'svm').exists():
        raise SystemExit('build first: ./spro build linux --arch %s --profile %s --board %s'
                         % (arch, profile, board))
    name = '%s-%s-linux-%s-%s' % (PROJECT['name'].lower(), PROJECT['version'], arch, board)
    pkg = OUT / name
    if pkg.exists():
        shutil.rmtree(pkg)
    pkg.mkdir(parents=True)
    for f in ('svm', 'kits.scode', 'app.sab', 'svm.conf'):
        shutil.copy2(run / f, pkg / f)
    if (run / 'manifests').exists():
        shutil.copytree(run / 'manifests', pkg / 'manifests')
    for f in ('install.sh', 'sedonapro@.service', 'realtime.conf.example'):
        shutil.copy2(INSTALL / f, pkg / f)
    shutil.copy2(ROOT / 'targets/linux/SVM_CONFIG_RU.md', pkg / 'SVM_CONFIG_RU.md')
    (pkg / 'VERSION').write_text('%s %s linux-%s profile %s board %s\n' % (
        PROJECT['name'], PROJECT['version'], arch, profile, board))
    (pkg / 'README.txt').write_text(
        '%s %s for Linux (%s), board %s\n\n'
        '  sudo ./install.sh            install/update instance "main" and start it\n'
        '  sudo ./install.sh --help     options (instances, --replace-app, --uninstall)\n\n'
        'SOX udp/1876 (admin password chosen at build time), web tcp/8080.\n'
        'An update keeps app.sab and svm.conf of the instance.\n'
        % (PROJECT['name'], PROJECT['version'], arch, board))
    names, known = select(profile, 'linux')
    libs = sorted({l for n in names for l in known[n].libs})
    return pkg, name, [DEB_DEPENDS[l] for l in libs if l in DEB_DEPENDS]


def tarball(pkg, name):
    path = OUT / (name + '.tar.gz')
    with tarfile.open(path, 'w:gz') as t:
        t.add(pkg, arcname=name)
    return path


def deb(pkg, arch, depends, board):
    version = PROJECT['version']
    root = OUT / 'deb-root'
    if root.exists():
        shutil.rmtree(root)
    payload = root / 'opt/sedonapro/package'
    shutil.copytree(pkg, payload)
    debian = root / 'DEBIAN'
    debian.mkdir()
    size = sum(p.stat().st_size for p in payload.rglob('*') if p.is_file()) // 1024
    (debian / 'control').write_text(
        'Package: sedonapro\nVersion: %s\nArchitecture: %s\nMaintainer: SedonaPro community\n'
        'Installed-Size: %d\nDepends: %s\nSection: misc\nPriority: optional\n'
        'Description: SedonaPro VM (Sedona Framework compatible) for board %s\n'
        ' Installs /opt/sedonapro/package and runs its install.sh: instance "main",\n'
        ' systemd unit sedonapro@main. Updates keep app.sab and svm.conf.\n'
        % (version, arch, size, ', '.join(['systemd', 'coreutils'] + depends), board))
    (debian / 'postinst').write_text(
        '#!/bin/sh\nset -e\nif [ "$1" = configure ]; then\n'
        '  /opt/sedonapro/package/install.sh --instance main\nfi\n')
    (debian / 'prerm').write_text(
        '#!/bin/sh\nset -e\nif [ "$1" = remove ]; then\n'
        '  systemctl disable --now sedonapro@main.service 2>/dev/null || true\nfi\n')
    (debian / 'postrm').write_text(
        '#!/bin/sh\nset -e\nif [ "$1" = purge ]; then\n'
        '  echo "SedonaPro: instance data kept in /opt/sedonapro/instances"\nfi\n')
    for f in ('postinst', 'prerm', 'postrm'):
        os.chmod(debian / f, 0o755)
    path = OUT / ('sedonapro_%s_%s.deb' % (version, arch))
    H.run(['fakeroot', 'dpkg-deb', '--build', '-Zxz', root, path])
    shutil.rmtree(root)
    return path


def package(arch='amd64', profile=None, board=None, make_deb=True):
    pkg, name, depends = stage(arch, profile, board)
    files = [tarball(pkg, name)]
    if make_deb and shutil.which('dpkg-deb') and shutil.which('fakeroot'):
        files.append(deb(pkg, arch, depends, board or PROJECT['targets']['linux']['default_board']))
    sums = OUT / 'SHA256SUMS'
    lines = {}
    if sums.exists():
        for line in sums.read_text().splitlines():
            h, n = line.split(None, 1)
            lines[n.strip()] = h
    for f in files:
        lines[f.name] = hashlib.sha256(f.read_bytes()).hexdigest()
    sums.write_text(''.join('%s  %s\n' % (h, n) for n, h in sorted(lines.items()) if (OUT / n).exists()))
    for f in files:
        print('PACKAGE %s (%d KB)' % (f, f.stat().st_size // 1024))
    return pkg


def docker_image(arch='amd64', profile=None, board=None, tag=None):
    pkg = package(arch, profile, board, make_deb=False)
    ctx = OUT / 'docker'
    if ctx.exists():
        shutil.rmtree(ctx)
    ctx.mkdir()
    shutil.copytree(pkg, ctx / 'package')
    for f in ('Dockerfile', 'entrypoint.sh'):
        shutil.copy2(DOCKER / f, ctx / f)
    tag = tag or 'sedonapro/svm:%s-%s' % (PROJECT['version'], board or PROJECT['targets']['linux']['default_board'])
    H.run(['docker', 'build', '-t', tag, ctx])
    print('IMAGE %s\n  docker run --rm -p 1876:1876/udp -p 8080:8080 -p 8090:8090 -v sedonapro-data:/data %s'
          % (tag, tag))
