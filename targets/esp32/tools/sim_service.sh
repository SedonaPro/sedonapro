#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
# ESP32 simulator as a systemd user service.
#   bash tools/sim_service.sh install|uninstall|restart|status|log
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
UNIT=$HOME/.config/systemd/user/esp32sim.service
case "${1:-status}" in
  install)
    [ -x "$ROOT/build/sim/esp32sim" ] || { echo "build it first: python3 build.py sim"; exit 1; }
    mkdir -p "$(dirname "$UNIT")"
    sed "s#@ROOT@#$ROOT#g" "$ROOT/tools/esp32sim.service" > "$UNIT"
    systemctl --user daemon-reload
    systemctl --user enable --now esp32sim.service
    # keep user services running without an open login session
    loginctl enable-linger "$(id -un)" 2>/dev/null \
      || echo "NOTE: linger is off; run: sudo loginctl enable-linger $(id -un)"
    systemctl --user --no-pager status esp32sim.service | head -5 ;;
  uninstall)
    systemctl --user disable --now esp32sim.service || true
    rm -f "$UNIT"
    systemctl --user daemon-reload ;;
  restart) systemctl --user restart esp32sim.service ;;
  status)  systemctl --user --no-pager status esp32sim.service ;;
  log)     journalctl --user -u esp32sim.service -n 100 --no-pager ;;
  *) echo "usage: $0 install|uninstall|restart|status|log"; exit 2 ;;
esac
