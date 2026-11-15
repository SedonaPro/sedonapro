# Цель esp32 и симулятор sim

Прошивка ESP32-S3 (ESP-IDF v6.0.3, FreeRTOS) с Sedona VM, SOX по сети и LittleFS
для `kits.scode`/`app.sab`. Состав kits — профиль `lite` (см. `profiles/lite.toml`).

```sh
./spro build sim                      # код прошивки как Linux-программа: build/sim/esp32sim
./spro run sim --erase                # «прошить» build/sim/flash и запустить; SOX 127.0.0.1:1876
./spro build sim --add serial,sproModbusMaster,sproModbusSlave,sproMqtt
./spro test sim                       # юнит-тесты, SOX, перезагрузка, watchdog, safeboot, лимит кучи

./spro build esp32                    # нативы + data (LittleFS) для прошивки, refSize 4
./spro fw esp32 --fw qemu --docker    # idf.py build в образе espressif/idf
./spro qemu esp32 --docker            # прошивка в QEMU, SOX на udp/1876 хоста
./spro fw esp32 --fw hw               # для платы: Wi-Fi через idf.py menuconfig
```

| Путь | Что это |
|---|---|
| `firmware/` | проект ESP-IDF: `app_main`, сеть, компонент `sedona`, Kconfig, разделы |
| `apps/esp32-demo.sax` | демо-приложение: платформа, SOX, рампы и сумматор |
| `apps/fragments/*.xml` | куски приложения; кусок добавляется, если все его kits есть в образе |
| `tests/sim_test.py` | проверки симулятора (`./spro test sim`) |
| `../../platforms/esp32/` | kit `sproPlatformEsp32` и `sys_esp32` (замена `sys/native/unix`) |
| `../../emu/esp32sim/` | заглушки ESP-IDF для симулятора |

Симулятор: `--heap` байт (сверх лимита `malloc` = NULL), `--wdt` с, `--restart-delay` мс,
`--max-resets`, `--no-restart`, `--tests`. `kill -USR2 <pid>` вешает цикл (проверка watchdog).
Не эмулируются: компилятор Xtensa, IRAM, Wi-Fi, скорость ядра.

Kits, которых пока нет на чипе: `sproMqtt` (нужен будильник вместо `pthread_kill`),
Modbus (libmodbus как компонент ESP-IDF и UART).
