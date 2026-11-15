#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
# First start: copy the packaged app into the /data volume; later starts keep
# the saved app.sab and svm.conf.  Extra arguments go to svm (--opt=key=value).
set -e
P=/opt/sedonapro/package
for f in kits.scode app.sab svm.conf; do
  [ -e "/data/$f" ] || cp "$P/$f" "/data/$f"
done
cp "$P/kits.scode" /data/kits.scode       # the image decides the kits
rm -rf /data/manifests && cp -r "$P/manifests" /data/manifests 2>/dev/null || true
exec "$P/svm" --plat "$@"
