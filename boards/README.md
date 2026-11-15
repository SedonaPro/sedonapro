# Платы

Плата — это один файл `boards/<имя>.toml`. В нём перечислены каналы ввода-вывода:
имя канала, его вид и где он физически находится. Компоненты kit `sproHal`
(`DigitalInput`, `DigitalOutput`, `AnalogInput`, `AnalogOutput`) обращаются к
каналу по имени, поэтому одно и то же приложение работает на любой плате, где
есть каналы с такими именами.

```sh
./spro boards                                    # какие платы есть
./spro board new my-plc --from rpi4              # новая плата из похожей
./spro build linux --board my-plc                # сборка под неё
```

## Формат

```toml
name    = "my-plc"
title   = "Мой контроллер"
hal     = "linux-gpiod"          # sim | linux-gpiod | linux-sysfs | esp32
targets = ["linux"]

[defaults]                       # необязательно: общее для всех каналов
dev = "gpiochip0"

[channels]
DI1 = { kind = "di", pin = 17, pull = "up", invert = true }
DO1 = { kind = "do", pin = 5, initial = 0 }
AI1 = { kind = "ai", dev = "iio:device0/in_voltage0_raw", raw_max = 4095, range = [0.0, 10.0], unit = "V" }
AO1 = { kind = "ao", dev = "pwmchip0", pin = 0, pwm_hz = 1000, range = [0.0, 100.0] }
```

| Ключ | Для | Что значит |
|---|---|---|
| `kind` | все | `di` дискретный вход, `do` дискретный выход, `ai` аналоговый вход, `ao` аналоговый выход (ШИМ) |
| `pin` | все | ESP32: номер GPIO; linux-gpiod: номер линии чипа; linux-sysfs: номер `/sys/class/gpio/gpioN`; ao на Linux: номер канала ШИМ |
| `dev` | linux | gpiod: `gpiochip0` или `/dev/gpiochipN`; ai: файл IIO; ao: `pwmchipN` или путь к нему |
| `pull` | di | `up`, `down`, `none` |
| `invert` | di, do | логическая 1 = электрический 0 (кнопка на землю, реле с активным нулём) |
| `raw_max` | ai | показание АЦП в верхней точке диапазона (ESP32: 4095) |
| `range` | ai, ao | инженерный диапазон `[lo, hi]`; для ao по умолчанию `[0, 100]` (проценты ШИМ) |
| `unit` | ai, ao | единицы для людей (`V`, `°C`, `%`) |
| `pwm_hz` | ao | частота ШИМ, по умолчанию 1000 |
| `initial` | do, ao, sim | значение при старте; на плате `sim` — начальное значение входа |

Имя канала: латинская буква, затем до 14 букв, цифр или `_`. Принятые имена:
`DI1…`, `DO1…`, `AI1…`, `AO1…`. Можно и смысловые: `BOOT`, `RELAY_FAN`.

## Последовательные порты

```toml
serial = ["/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttymxc1"]
```

Номер порта в Modbus и других последовательных протоколах — позиция в списке:
1 = `/dev/ttyUSB0`, 3 = `/dev/ttymxc1`. Без `serial` действует список по умолчанию
(`ttyUSB0`, `ttyUSB1`, `ttyS0`, `ttyS1`). Плате, у которой есть только порты
(`imx6`, `wecon-pi8070`), каналы не нужны.

## Бэкенды

| `hal` | Где | Как работает |
|---|---|---|
| `sim` | linux, sim | виртуальные пины; веб-панель `http://127.0.0.1:8090` (`SPRO_GPIO_PORT`, 0 = выкл.) |
| `linux-gpiod` | linux | символьное устройство GPIO ядра (uAPI v2, Linux 5.10+), без библиотек; ai через IIO, ao через sysfs PWM |
| `linux-sysfs` | linux | старый `/sys/class/gpio` для старых ядер и образов |
| `esp32` | esp32 | драйверы ESP-IDF: gpio, adc_oneshot, ledc (до 8 ШИМ, до 4 разных частот) |

Новый способ доступа к железу (I²C-расширитель, Modbus-модуль ввода-вывода) — это
новый бэкенд: каталог `kits/io/sproHal/backends/<имя>/` с тремя функциями из
`kits/io/sproHal/native/spro_hal.h` (`open`, `read`, `write` на сырых значениях)
и строка в `BACKENDS` в `tools/spro/boards.py`. Инверсию и масштабирование делает
общий код, бэкенду о них знать не нужно.
