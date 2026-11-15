# SedonaPro

Открытая сборка среды исполнения, совместимой с Sedona Framework, для
контроллеров автоматизации: от ESP32 до промышленных Linux-контроллеров.
Набор kits разложен по областям, сборка идёт по профилям, всё можно запустить
и проверить на обычном ПК без железа.

*English: [README_EN.md](README_EN.md), [quick start](docs/quickstart_EN.md).*

## Быстрый старт

Новичкам — [docs/quickstart_RU.md](docs/quickstart_RU.md): контроллер на ПК за 10 минут.

Нужны Python 3.11+, JDK 8+ и gcc. Для полного профиля ещё `libmodbus-dev` и `libsqlite3-dev`.

```sh
./spro kits                    # какие kits есть и где они работают (подробно: docs/kits.md)
./spro profiles                # профили lite, full, pro
./spro packs                   # отраслевые наборы kits: --pack control, pumps, hvac
./spro build linux             # svm + kits.scode + app.sab -> build/linux-amd64-pro-sim/run
./spro test linux              # smoke-тест (старт, web, ввод-вывод) + юнит-тесты Sedona
./spro package linux           # tar.gz с install.sh и .deb -> build/packages
./spro run linux               # запустить VM: SOX udp/1876, web tcp/8080

./spro build sim               # прошивка ESP32 как программа для ПК
./spro run sim --erase         # SOX udp/1876, панель GPIO http://127.0.0.1:8090
./spro test sim

./spro kit new acmeBoiler --domain hvac --vendor Acme   # свой kit из шаблона
./spro catalog                 # обновить каталог docs/kits.md
./spro ui check                # виджеты kits (docs/widgets.md)
./spro editor build && ./spro editor run   # веб-редактор: http://127.0.0.1:8098
./spro broker                  # MQTT-брокер для опытов; ./spro pub ТОПИК ЗНАЧЕНИЕ
```

Пароль `admin` в отладочной сборке — `sedonapro-dev`. Для своей сборки задайте
`SEDONA_ADMIN_PASSWORD`.

## Устройство репозитория

| Путь | Что там |
|---|---|
| `core/` | Sedona Framework (Tridium, AFL 3.0): VM, базовые kits, компилятор `sedonac`, документация |
| `kits/<область>/` | kits SedonaPro: `base`, `control`, `equip`, `alarm`, `sched`, `hvac`, `io`, `comm`, `data`, `logic`, `ui`, `sim` (правила pro-kits: `docs/pro-kits.md`) |
| `platforms/` | платформенные kits: `linux`, `esp32` |
| `boards/` | платы: какие каналы ввода-вывода есть и на каких пинах (`boards/README.md`) |
| `profiles/` | `lite.toml` (ESP32), `full.toml` (Linux, классический набор — подключается любой редактор Sedona), `pro.toml` (Linux по умолчанию: full + pro-kits; клиенту SOX нужны пакеты 1024 байт, как в Sedona Studio и веб-редакторе); `packs/` — отраслевые наборы |
| `targets/` | сборка под цель: `linux` (svm, установщик, Docker), `esp32` (прошивка ESP-IDF) — см. их README |
| `emu/` | эмуляция на ПК: `esp32sim`, 32-битный тулчейн, QEMU — см. `emu/README.md` |
| `examples/` | примеры приложений (`--example blink,thermostat`) |
| `docs/` | руководства |
| `editor/` | веб-редактор (Java-сервер + React); настольный редактор Sedona Studio — отдельный репозиторий `sedona-studio` |
| `tools/spro/` | утилита `./spro` |
| `registry/` | реестры номеров нативных kits и префиксов авторов |
| `integrations/` | интеграции со сторонними системами, в публичную часть не входят |

У каждого kit есть `kit.xml` (Sedona) и, если нужно, `kit.toml`: на каких целях он
собирается, какие нативные исходники приносит, с какими библиотеками линкуется.
Зависимости между kits берутся из `kit.xml` автоматически.

## Kits

Префикс наших kits — `spro`, vendor — `SPro`. Сторонние авторы берут свой префикс
и записывают его в `registry/vendors.md`.

| Область | Kits |
|---|---|
| base | `sproBase` (основа pro-kits), `sproSignal` (4–20 мА, Pt/Ni/NTC, фильтры, достоверность), `sproLib`, `sproMath`, `sproUtil` |
| io | `sproHal` (ввод-вывод по имени канала), `sproIo`, `sproIoLinux` (последовательные порты платы), `sproSpi` |
| comm | `sproModbusMaster`, `sproModbusSlave`, `sproMqtt`, `sproSerialProtocol` |
| data | `sproSqlite` |
| control | `sproControl` (ПИД, схемы ограничения, секвенции, ступени, 3-позиционные приводы) |
| equip | `sproEquip` (насосы, вентиляторы: защиты, основной/резервный, ротация, каскад со «сном»), `sproPumpSt` (конфигуратор насосной станции с виджетом) |
| alarm | `sproAlarm` (пределы, защиты с автоповтором, общая авария, журнал) |
| sched | `sproSched` (праздники, недельное расписание с программами дней, продление, оптимальный пуск) |
| hvac | `sproHvac` (вентустановки: сезон, последовательности пуска, каскад температуры, водяной калорифер с защитами, электрокалорифер, рекуператор, вентиляторы), `sproAhu` (конфигуратор установки: выбрал состав — получил блоки, входы-выходы и связи), `sproHumid` (влажный воздух: точка росы, энтальпия; увлажнение с ограничением, осушение, энтальпийный экономайзер) |
| heat | `sproHeat` (ИТП: отопительный график, контур отопления с ограничением обратки и защитой от замерзания, ГВС с дезинфекцией и приоритетом, подпитка), `sproHeatSt` (конфигуратор ИТП с виджетом) |
| energy | `sproEnergy` (учёт по тарифным зонам с итогами за сутки и месяц, расчётный теплосчётчик, 30-минутная мощность с отключением нагрузок, АВР с ДГУ) |
| logic | `sproExpr` (формулы) |
| ui | `sproUiConnect` |
| sim | `sproSim` (модели объектов для обучения и проверки алгоритмов) |

Полный список с компонентами и слотами — [docs/kits.md](docs/kits.md).

## Состояние

- [x] Этап 1: структура, лицензии, импорт ядра, сборка и тесты Linux
- [x] Этап 2: перенос сборки ESP32 и `esp32sim` в `./spro`
- [x] Этап 3: HAL GPIO (`sproHal`) и описания плат `boards/*.toml`
- [x] Этап 4: установщик VM: `./spro package linux` (`install.sh`, .deb), `./spro docker linux` — проверено в Debian 12 + systemd (`./spro test install`) и в Docker
- [x] Этап 5: эмуляция: панель GPIO, svm в Docker, ESP32-S3 в QEMU (`./spro test esp32`), 32-битные сборки — `emu/README.md`
- [x] Этап 6: CI (`.github/workflows/ci.yml`, проверен локально на чистом клоне), CONTRIBUTING,
  первые шаги, примеры blink и thermostat, шаблон kit, каталог `docs/kits.md`
- [ ] Проверка на железе: ESP32-S3, Raspberry Pi / BeagleBone (`linux-gpiod`)
- [ ] Редактор

## Лицензии

Ядро — Academic Free License 3.0, kits SedonaPro — Apache 2.0, сторонние части —
см. `NOTICE` и `LICENSES/`. SedonaPro не связан с Tridium, Inc.
