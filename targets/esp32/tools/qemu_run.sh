#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
# Run the built firmware in Espressif QEMU (inside the espressif/idf image or
# with ESP-IDF in the environment).  Serial console on stdout, no monitor.
#   qemu_run.sh <fw build dir> [sox host port, default 1876]
# idf.py adds the default eFuse image, 32 MB and disables the TIMG watchdog.
set -eu
B=$1
PORT=${2:-1876}
FW=$(cd "$(dirname "$0")/../firmware" && pwd)
exec idf.py -C "$FW" -B "$B" -DSDKCONFIG="$B/sdkconfig" qemu \
  --qemu-extra-args "-nic user,model=open_eth,hostfwd=udp::$PORT-:1876"
