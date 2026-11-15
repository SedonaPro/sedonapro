# Цель linux

Sedona VM (`svm`) для Linux-контроллеров (x86-64, ARM) и для ПК. Состав kits —
профиль `full`, ввод-вывод — файл платы (`--board`, по умолчанию виртуальная `sim`).

## Сборка и запуск на ПК

```sh
./spro build linux                       # build/linux-amd64-pro-sim/run
./spro test linux                        # юнит-тесты Sedona (~5 мин)
./spro run linux                         # SOX udp/1876, web :8080, панель GPIO :8090
./spro build linux --board rpi4 --arch arm64 --cc aarch64-linux-gnu-gcc
```

`--fast` собирает VM с `-O3` и computed goto, `--asan` — диагностическая сборка
с AddressSanitizer (только для тестов). Пароль `admin` задаёт `SEDONA_ADMIN_PASSWORD`.

## Установка на контроллер

```sh
./spro package linux --board rpi4 --arch arm64
#   build/packages/sedonapro-<ver>-linux-arm64-rpi4.tar.gz   (+ .deb, SHA256SUMS)
```

На контроллере (systemd):

```sh
tar xzf sedonapro-*.tar.gz && cd sedonapro-*/
sudo ./install.sh                  # экземпляр main: /opt/sedonapro/instances/main
sudo ./install.sh --instance lab2  # ещё один экземпляр (свои порты в app.sab)
sudo ./install.sh --uninstall
```

или пакетом: `sudo apt install ./sedonapro_<ver>_arm64.deb`.

Обновление сохраняет `app.sab` и `svm.conf` экземпляра (`--replace-app` — заменить),
прежние файлы кладёт в `/opt/sedonapro/backups/<экземпляр>-<время>/`.
Служба `sedonapro@<экземпляр>`: `Type=notify`, watchdog, пользователь `sedona`,
доступ только к GPIO, последовательным портам и SPI. Настройки VM — `svm.conf`
(`SVM_CONFIG_RU.md`), режим реального времени — `realtime.conf.example`.

## Docker

```sh
./spro docker linux                       # образ sedonapro/svm:<ver>-sim
docker run --rm -p 1876:1876/udp -p 8080:8080 -p 8090:8090 -v sedonapro-data:/data sedonapro/svm:0.1.0-sim
```

Приложение и настройки живут в томе `/data`; набор kits задаёт образ.

## Файлы

| Путь | Что это |
|---|---|
| `apps/*.sax` | приложения по умолчанию (`--app`) |
| `install/` | `install.sh`, unit `sedonapro@.service`, `realtime.conf.example` |
| `docker/` | `Dockerfile`, `entrypoint.sh` |
| `svm.conf.example`, `SVM_CONFIG_RU.md` | настройки VM |
| `tests/` | функциональные проверки VM (перенос в `./spro test` — в работе) |
