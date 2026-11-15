# Quick start

In 10 minutes: build a controller on your PC, "press a button" and watch a
Sedona program drive the outputs. No hardware needed.

## 1. Install

Linux (Debian/Ubuntu) or WSL2:

```sh
sudo apt install python3 openjdk-17-jdk-headless gcc libmodbus-dev libsqlite3-dev git
git clone <repository url> sedonapro && cd sedonapro
```

## 2. An ESP32 controller on your PC

```sh
./spro build sim --example blink,thermostat
./spro run sim --erase
```

This is the real ESP32 firmware code built as a PC program. Open
`http://127.0.0.1:8090`, the I/O panel of the virtual board:

- **DO2** blinks once a second: the `blink` example (timer → output);
- move **AI2** (temperature) below 20.5 °C and **DO3** (heater) turns on, above
  21.5 °C it turns off: the `thermostat` example;
- press **DI1** and **DO1** lights up.

Stop with Ctrl+C. `--erase` starts from a clean "flash"; without it the
controller keeps saved changes, like a real one.

## 3. How it works

A Sedona program is components and links between them. `examples/thermostat.xml`:

```
AnalogInput (channel AI2) ──out→cv── Tstat (setpoint 21, diff 1) ──out→in── DigitalOutput (channel DO3)
```

I/O components know only the channel name. Which pin it is comes from the board
file: `boards/sim.toml` on a PC, `boards/esp32-s3-devkitc.toml` on an ESP32. So the
same program runs unchanged on a real controller.

## 4. Connect an editor

The controller listens for SOX on `udp/1876` (user `admin`, development password
`sedonapro-dev`). Any Sedona editor with SOX support works; it downloads the kit
descriptions from the controller.

## 5. Next

| I want to | Look at |
|---|---|
| A Linux controller on the PC, install on a real one | `targets/linux/README.md` |
| Flash an ESP32-S3 | `targets/esp32/README.md` |
| Run the ESP32 firmware in QEMU | `./spro qemu esp32 --docker`, `emu/README.md` |
| Connect to MQTT, Home Assistant, Node-RED | example `mqtt` (`examples/README.md`), `kits/comm/sproMqtt/README.md` |
| My own board | `./spro board new myboard --from rpi4`, `boards/README.md` |
| My own example | `examples/README.md` |
| My own kit | `./spro kit new acmeX --domain logic --vendor Acme`, then `CONTRIBUTING.md` |
| Which kits exist | `docs/kits.md` (components and slots), `./spro kits` |
