# Первые шаги

За 10 минут: собрать контроллер на своём ПК, «нажать кнопку» и увидеть, как
программа на Sedona управляет выходами. Железо не нужно.

## 1. Что поставить

Linux (Debian/Ubuntu) или WSL2:

```sh
sudo apt install python3 openjdk-17-jdk-headless gcc libmodbus-dev libsqlite3-dev git
git clone <адрес репозитория> sedonapro && cd sedonapro
```

## 2. Контроллер ESP32 у вас на ПК

```sh
./spro build sim --example blink,thermostat
./spro run sim --erase
```

Это настоящий код прошивки ESP32, собранный как программа для ПК. Откройте
`http://127.0.0.1:8090` — это панель ввода-вывода виртуальной платы:

- **DO2** мигает раз в секунду — пример `blink` (таймер → выход);
- сдвиньте **AI2** (температура) ниже 20,5 °C — включится **DO3** (нагреватель),
  выше 21,5 °C — выключится. Это пример `thermostat`;
- нажмите **DI1** — загорится **DO1**.

Остановить: Ctrl+C. `--erase` начинает с чистой «флешки»; без него контроллер
помнит сохранённые изменения, как настоящий.

## 3. Как это устроено

Программа Sedona — это компоненты и связи между ними. Пример `examples/thermostat.xml`:

```
AnalogInput (канал AI2) ──out→cv── Tstat (уставка 21, гистерезис 1) ──out→in── DigitalOutput (канал DO3)
```

Компоненты ввода-вывода знают только имя канала. Какой это пин, решает файл платы:
на ПК — `boards/sim.toml`, на ESP32 — `boards/esp32-s3-devkitc.toml`. Поэтому та же
программа без изменений работает на настоящем контроллере.

## 4. Подключиться редактором

Контроллер слушает SOX на `udp/1876` (пользователь `admin`, пароль сборки
`sedonapro-dev`). Подойдёт любой редактор Sedona с поддержкой SOX: он сам загрузит
описания kits с контроллера.

Редакторов в проекте два, они независимы:

| Редактор | Что это | Где |
|---|---|---|
| Веб-редактор | Java-сервер + React, работает в браузере | `editor/` |
| Sedona Studio | настольный редактор на Java 17 / JavaFX, без сервера | отдельный репозиторий `sedona-studio` (рядом с `sedonapro`) |

## 5. Дальше

| Хочу | Куда смотреть |
|---|---|
| Linux-контроллер на ПК, установка на настоящий | `targets/linux/README.md` |
| Прошить ESP32-S3 | `targets/esp32/README.md` |
| Прошивка ESP32 в QEMU | `./spro qemu esp32 --docker`, `emu/README.md` |
| Связать с MQTT, Home Assistant, Node-RED | пример `mqtt` (`examples/README.md`), `kits/comm/sproMqtt/README.md` |
| Своя плата | `./spro board new myboard --from rpi4`, `boards/README.md` |
| Свой пример | `examples/README.md` |
| Свой kit | `./spro kit new acmeX --domain logic --vendor Acme`, затем `CONTRIBUTING.md` |
| Какие kits есть | `docs/kits.md` (компоненты и слоты), `./spro kits` |
