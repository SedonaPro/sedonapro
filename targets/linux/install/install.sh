#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
# SedonaPro VM installer: run as root from the unpacked package directory.
#
#   sudo ./install.sh                    install or update instance "main" and start it
#   sudo ./install.sh --instance lab2    another instance (needs its own ports in app.sab)
#   sudo ./install.sh --no-start         install, do not (re)start
#   sudo ./install.sh --replace-app      also replace app.sab/svm.conf of an existing instance
#   sudo ./install.sh --uninstall        stop and remove the instance (backup is kept)
#
# Layout: /opt/sedonapro/instances/<instance>/{svm,kits.scode,app.sab,svm.conf,manifests}
#         /opt/sedonapro/backups/<instance>-<time>/   previous files on every update
#         /etc/systemd/system/sedonapro@.service      (+ sedonapro@<instance>.service.d/)
# An update keeps the instance's app.sab and svm.conf: the application belongs to the user.
set -eu

PREFIX=/opt/sedonapro
INSTANCE=main
START=1
REPLACE_APP=0
UNINSTALL=0
HERE=$(cd "$(dirname "$0")" && pwd)

while [ $# -gt 0 ]; do
  case "$1" in
    --instance) INSTANCE=$2; shift 2 ;;
    --instance=*) INSTANCE=${1#*=}; shift ;;
    --no-start) START=0; shift ;;
    --replace-app) REPLACE_APP=1; shift ;;
    --uninstall) UNINSTALL=1; shift ;;
    -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
    *) echo "unknown option $1" >&2; exit 2 ;;
  esac
done

case "$INSTANCE" in
  *[!A-Za-z0-9_-]*|'') echo "instance name: letters, digits, _ and - only" >&2; exit 2 ;;
esac
[ "$(id -u)" = 0 ] || { echo "run as root: sudo $0 $*" >&2; exit 1; }
command -v systemctl >/dev/null || { echo "systemd is required" >&2; exit 1; }

DIR=$PREFIX/instances/$INSTANCE
UNIT=sedonapro@$INSTANCE.service
STAMP=$(date +%Y%m%d-%H%M%S)
BACKUP=$PREFIX/backups/$INSTANCE-$STAMP

backup() {
  if [ -d "$DIR" ]; then
    mkdir -p "$BACKUP"
    for f in svm kits.scode app.sab svm.conf; do
      [ -e "$DIR/$f" ] && cp -p "$DIR/$f" "$BACKUP/"
    done
    echo "backup: $BACKUP"
  fi
}

if [ "$UNINSTALL" = 1 ]; then
  systemctl disable --now "$UNIT" 2>/dev/null || true
  backup
  rm -rf "$DIR" "/etc/systemd/system/$UNIT.d"
  systemctl daemon-reload
  echo "removed instance $INSTANCE (unit template and other instances are kept)"
  exit 0
fi

for f in svm kits.scode app.sab svm.conf sedonapro@.service; do
  [ -e "$HERE/$f" ] || { echo "missing $f: run install.sh from the package directory" >&2; exit 1; }
done

# runtime libraries the svm was linked with
missing=$(ldd "$HERE/svm" 2>/dev/null | awk '/not found/ {print $1}')
if [ -n "$missing" ]; then
  echo "missing libraries: $missing" >&2
  echo "Debian/Ubuntu: apt install libmodbus5 libsqlite3-0" >&2
  exit 1
fi

# service user and the groups that own GPIO, serial and SPI devices
id sedona >/dev/null 2>&1 || useradd --system --home-dir "$PREFIX" --no-create-home \
  --shell /usr/sbin/nologin sedona
groups=""
for g in gpio dialout spi i2c; do
  getent group "$g" >/dev/null && groups="$groups $g"
done

was_active=0
systemctl is-active --quiet "$UNIT" && was_active=1
[ "$was_active" = 1 ] && systemctl stop "$UNIT"
backup

mkdir -p "$DIR"
install -m 0755 "$HERE/svm" "$DIR/svm"
install -m 0644 "$HERE/kits.scode" "$DIR/kits.scode"
for f in app.sab svm.conf; do
  if [ ! -e "$DIR/$f" ] || [ "$REPLACE_APP" = 1 ]; then
    install -m 0640 "$HERE/$f" "$DIR/$f"
  else
    echo "kept $DIR/$f (use --replace-app to overwrite)"
  fi
done
if [ -d "$HERE/manifests" ]; then
  rm -rf "$DIR/manifests"
  cp -r "$HERE/manifests" "$DIR/manifests"
fi
[ -f "$HERE/VERSION" ] && install -m 0644 "$HERE/VERSION" "$DIR/VERSION"
chown -R sedona:sedona "$DIR"
chmod 0750 "$DIR"

install -m 0644 "$HERE/sedonapro@.service" /etc/systemd/system/sedonapro@.service
mkdir -p "/etc/systemd/system/$UNIT.d"
if [ -n "$groups" ]; then
  printf '[Service]\nSupplementaryGroups=%s\n' "${groups# }" > "/etc/systemd/system/$UNIT.d/groups.conf"
fi
if [ -f "$HERE/realtime.conf.example" ]; then
  install -m 0644 "$HERE/realtime.conf.example" "/etc/systemd/system/$UNIT.d/realtime.conf.example"
fi
systemctl daemon-reload
systemctl enable "$UNIT" >/dev/null

if [ "$START" = 1 ]; then
  systemctl restart "$UNIT"
  if systemctl is-active --quiet "$UNIT"; then
    echo "started $UNIT"
  else
    echo "$UNIT did not start; see: journalctl -u $UNIT -n 50" >&2
    exit 1
  fi
fi
echo "installed $(cat "$HERE/VERSION" 2>/dev/null || echo SedonaPro) as $DIR"
echo "logs: journalctl -u $UNIT -f"
if [ -d "$BACKUP" ]; then echo "previous files: $BACKUP"; fi
