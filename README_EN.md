# SedonaPro

An open, community distribution of a Sedona Framework compatible runtime for
automation controllers, from ESP32 to industrial Linux controllers. Kits are
organized by domain, images are built from profiles, and everything runs and is
tested on an ordinary PC without hardware.

*Русская версия: [README_RU.md](README_RU.md).*

## Quick start

New to Sedona? Start with [docs/quickstart_EN.md](docs/quickstart_EN.md): a controller on your PC in 10 minutes.

Requires Python 3.11+, JDK 8+ and gcc; the `full` profile also needs `libmodbus-dev` and `libsqlite3-dev`.

```sh
./spro kits                    # kits and where they run (details: docs/kits.md)
./spro profiles                # lite, full and pro profiles
./spro packs                   # kit sets for a trade: --pack control, pumps, hvac
./spro build linux             # svm + kits.scode + app.sab -> build/linux-amd64-pro-sim/run
./spro test linux              # smoke test (start, web, I/O) + Sedona unit tests
./spro package linux           # tar.gz with install.sh and a .deb -> build/packages
./spro run linux               # start the VM: SOX udp/1876, web tcp/8080

./spro build sim               # the ESP32 firmware code as a PC program
./spro run sim --erase         # SOX udp/1876, GPIO panel http://127.0.0.1:8090
./spro test sim

./spro kit new acmeBoiler --domain hvac --vendor Acme   # your own kit from a template
./spro catalog                 # regenerate docs/kits.md
./spro ui check                # widgets shipped with kits (docs/widgets.md)
./spro editor build && ./spro editor run   # web editor: http://127.0.0.1:8098
./spro broker                  # MQTT broker for experiments; ./spro pub TOPIC VALUE
```

The development `admin` password is `sedonapro-dev`; set `SEDONA_ADMIN_PASSWORD`
for your own builds.

## Repository layout

| Path | Contents |
|---|---|
| `core/` | Sedona Framework (Tridium, AFL 3.0): VM, core kits, the `sedonac` compiler, docs |
| `kits/<domain>/` | SedonaPro kits: `base`, `control`, `equip`, `alarm`, `sched`, `hvac`, `io`, `comm`, `data`, `logic`, `ui`, `sim` (pro kit rules: `docs/pro-kits.md`) |
| `platforms/` | platform kits: `linux`, `esp32` |
| `boards/` | boards: which I/O channels exist and on which pins (`boards/README.md`) |
| `profiles/` | `lite.toml` (ESP32), `full.toml` (Linux, the classic set: any Sedona tool connects), `pro.toml` (Linux default: full + pro kits; SOX clients need 1024-byte packets, as Sedona Studio and the web editor use); `packs/` — kit sets for a trade |
| `targets/` | per-target build: `linux` (svm, installer, Docker), `esp32` (ESP-IDF firmware) |
| `emu/` | emulation on a PC: `esp32sim`, 32-bit toolchain, QEMU (`emu/README.md`) |
| `examples/` | example applications (`--example blink,thermostat`) |
| `docs/` | guides and the kit catalog |
| `editor/` | web editor (Java server + React) |
| `tools/spro/` | the `./spro` tool |
| `registry/` | native kit ids and vendor prefixes |

Every kit has a `kit.xml` (Sedona) and optionally a `kit.toml`: targets it builds
for, native sources, libraries. Dependencies between kits come from `kit.xml`.

## Kits

Our kits use the prefix `spro` and vendor `SPro`. Other authors pick their own
prefix and register it in `registry/vendors.md`. The full list with components
and slots is in [docs/kits.md](docs/kits.md).

| Domain | Kits |
|---|---|
| base | `sproBase` (foundation of the pro kits), `sproSignal` (4-20 mA, Pt/Ni/NTC, filters, validation), `sproLib`, `sproMath`, `sproUtil` |
| io | `sproHal` (I/O by channel name), `sproIo`, `sproIoLinux` (serial ports of the board), `sproSpi` |
| comm | `sproModbusMaster`, `sproModbusSlave`, `sproMqtt`, `sproSerialProtocol` |
| data | `sproSqlite` |
| control | `sproControl` (PID, override selection, sequences, stages, 3-point actuators) |
| equip | `sproEquip` (pumps, fans: protections, duty/standby, rotation, cascade with sleep), `sproPumpSt` (pump station configurator with a widget) |
| alarm | `sproAlarm` (limits, trips with retries, common alarm, journal) |
| sched | `sproSched` (holiday calendar, weekly schedule with day programs, extension, optimum start) |
| hvac | `sproHvac` (air handling: season, start/stop sequences, temperature cascade, water heater with frost protection, electric heater, heat recovery, fans), `sproAhu` (unit configurator: pick the parts, get blocks, I/O and links), `sproHumid` (humid air: dew point, enthalpy; humidifier with high limit, dehumidification, enthalpy economizer) |
| heat | `sproHeat` (heating substations: outdoor-reset curve, heating circuit with return limit and frost protection, hot water with disinfection and priority, make-up), `sproHeatSt` (heating substation configurator with a widget) |
| energy | `sproEnergy` (meters with tariff zones and day/month totals, calculated heat meter, demand with load shedding, automatic transfer switch with generator) |
| logic | `sproExpr` (formulas) |
| ui | `sproUiConnect` |
| sim | `sproSim` (plant models for training and testing control algorithms) |

## Hardware abstraction

I/O components know only a channel name (`DI1`, `DO1`, `AI1`, `AO1`). A board
file `boards/<name>.toml` maps channels to pins, and a HAL backend does the
access: `sim` (virtual pins with a web panel), `linux-gpiod` (GPIO character
device, IIO, sysfs PWM), `linux-sysfs`, `esp32` (ESP-IDF gpio, ADC, LEDC).
Porting to a new board is usually one file: `./spro board new myboard --from rpi4`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).
CI builds and tests the Linux VM, the ESP32 simulator and the ESP32-S3 firmware in QEMU.

## License

Core: Academic Free License 3.0; SedonaPro kits and tools: Apache 2.0; third-party
parts: see `NOTICE` and `LICENSES/`. "Sedona Framework" is a name of Tridium, Inc.;
SedonaPro is not affiliated with or endorsed by Tridium.
