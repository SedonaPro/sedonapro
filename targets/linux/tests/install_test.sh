#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
# install.sh and .deb in a throwaway Debian 12 + systemd container.
# usage: install_test.sh build/packages   (after ./spro package linux)
set -eu
PKGS=$(cd "$1" && pwd)
HERE=$(cd "$(dirname "$0")" && pwd)
IMG=sedonapro/systemd-test
C=spro-install-test
TAR=$(ls "$PKGS"/sedonapro-*-linux-amd64-*.tar.gz | head -1)
DEB=$(ls "$PKGS"/sedonapro_*_amd64.deb | head -1)

docker build -q -t $IMG "$HERE/systemd" >/dev/null
docker rm -f $C >/dev/null 2>&1 || true
docker run -d --name $C --privileged --cgroupns=host -v /sys/fs/cgroup:/sys/fs/cgroup:rw \
  --tmpfs /run --tmpfs /run/lock $IMG >/dev/null
trap 'docker rm -f $C >/dev/null 2>&1' EXIT
x() { docker exec $C sh -c "$1"; }
ok() { echo "PASS $1"; }
fail() { echo "FAIL $1"; x "journalctl -u sedonapro@main -n 40 --no-pager" || true; exit 1; }
waitweb() { for i in $(seq 1 30); do x "curl -sf -o /dev/null http://127.0.0.1:8080/" && return 0; sleep 1; done; return 1; }

for i in $(seq 1 30); do x "systemctl is-system-running" 2>/dev/null | grep -qE "running|degraded" && break; sleep 1; done
docker cp "$TAR" $C:/root/pkg.tar.gz
docker cp "$DEB" $C:/root/pkg.deb
x "cd /root && tar xzf pkg.tar.gz"
D=$(basename "$TAR" .tar.gz)

x "/root/$D/install.sh" >/dev/null || fail "install.sh"
x "systemctl is-active sedonapro@main" >/dev/null || fail "service active"
waitweb || fail "web after install"
ok "install.sh: sedonapro@main active, web :8080"

x "id sedona && stat -c %U /opt/sedonapro/instances/main/app.sab" | grep -q sedona || fail "owner sedona"
ok "runs as sedona"

x "echo user-change > /opt/sedonapro/instances/main/svm.conf.marker && cp /opt/sedonapro/instances/main/app.sab /tmp/app.before && printf '\n# user setting\n' >> /opt/sedonapro/instances/main/svm.conf"
x "/root/$D/install.sh" >/dev/null || fail "update"
x "grep -q 'user setting' /opt/sedonapro/instances/main/svm.conf && cmp -s /tmp/app.before /opt/sedonapro/instances/main/app.sab" || fail "update keeps app.sab/svm.conf"
x "ls /opt/sedonapro/backups | grep -q main-" || fail "backup"
waitweb || fail "web after update"
ok "update keeps app.sab and svm.conf, backup made, service back"

x "/root/$D/install.sh --instance lab2 --no-start" >/dev/null || fail "second instance"
x "systemctl is-enabled sedonapro@lab2" >/dev/null || fail "lab2 enabled"
x "/root/$D/install.sh --instance lab2 --uninstall" >/dev/null || fail "uninstall lab2"
x "test ! -d /opt/sedonapro/instances/lab2 && systemctl is-active sedonapro@main" >/dev/null || fail "uninstall keeps main"
ok "second instance installed and removed, main untouched"

x "kill -SEGV \$(systemctl show -p MainPID --value sedonapro@main)"
sleep 6
x "systemctl is-active sedonapro@main" >/dev/null || fail "restart after crash"
waitweb || fail "web after crash"
ok "crash -> systemd restart"

x "/root/$D/install.sh --uninstall" >/dev/null
x "apt-get install -y -qq /root/pkg.deb" >/dev/null 2>&1 || fail "apt install deb"
x "systemctl is-active sedonapro@main" >/dev/null || fail "deb: service active"
waitweb || fail "deb: web"
x "dpkg -r sedonapro" >/dev/null 2>&1 || fail "dpkg -r"
x "! systemctl is-active sedonapro@main" >/dev/null || fail "deb removed: service stopped"
ok ".deb: install starts sedonapro@main, removal stops it"
echo "install: 0 failed"
