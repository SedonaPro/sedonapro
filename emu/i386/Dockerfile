# 32-bit (refSize 4) Linux toolchain: runs the same VM code paths as ESP32
# and 32-bit ARM controllers, with gdb.  Used by emu/i386/cc and ./spro.
FROM i386/debian:12-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends gcc libc6-dev gdb libmodbus-dev libsqlite3-dev \
 && rm -rf /var/lib/apt/lists/*
