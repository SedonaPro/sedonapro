#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
# Install ESP-IDF v6.0.3 for ESP32-S3 and Espressif QEMU into ~/esp.
# Downloads ~2-3 GB from github.com/espressif and dl.espressif.com.
#   bash tools/install_idf.sh
# Then in every new shell:  . ~/esp/esp-idf-v6.0.3/export.sh
set -euo pipefail
VER=v6.0.3
DIR=$HOME/esp/esp-idf-$VER
# python3 must have venv (Debian: apt install python3-venv; conda works too)
python3 -c "import venv, ensurepip" || { echo "python3 has no venv/ensurepip"; exit 1; }
mkdir -p "$HOME/esp"
if [ ! -d "$DIR" ]; then
  git clone --depth 1 --branch $VER --recursive --shallow-submodules -j8 \
      https://github.com/espressif/esp-idf.git "$DIR"
fi
cd "$DIR"
./install.sh esp32s3
python3 tools/idf_tools.py install qemu-xtensa
echo
echo "ESP-IDF $VER installed. Before building:  . $DIR/export.sh"
