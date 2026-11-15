# Эмуляция

Всё, что нужно, чтобы проверить SedonaPro без железа.

| Что | Команда | Насколько близко к железу |
|---|---|---|
| ESP32 как программа ПК (`esp32sim`) | `./spro build sim && ./spro run sim` | код прошивки (VM, нативы, платформа ESP32) с заглушками ESP-IDF; секунды на сборку, есть watchdog, safeboot, лимит кучи |
| ESP32 32-битный | `./spro build sim --arch i386 --cc emu/i386/cc` | то же, но refSize 4, как на чипе |
| ESP32-S3 в QEMU, автотест | `./spro fw esp32 --fw qemu --docker && ./spro test esp32` | настоящая прошивка ESP-IDF v6 на эмулированном Xtensa, сеть через OpenCores Ethernet |
| ESP32-S3 в QEMU вручную | `./spro qemu esp32 --docker` | консоль прошивки в терминале, SOX на `udp/1876`, Ctrl+C — стоп |
| Linux-контроллер | `./spro build linux && ./spro run linux` | svm как на контроллере, плата `sim` |
| Linux 32-бит | `./spro build linux --arch i386 --cc emu/i386/cc` | refSize 4, как на 32-битных ARM |
| Установка на контроллер | `./spro package linux && ./spro test install` | Debian 12 + systemd в контейнере |
| Ввод-вывод | панель `http://127.0.0.1:8090` (плата `sim`) | кнопки, ползунки, лампы |

Симулятор слушает те же порты, что и контроллер: SOX `udp/1876`, web `8080`, UI `8088`,
Modbus TCP `1502`. Если они заняты (QEMU, svm, другая программа), переназначьте их:
`ESP32SIM_PORTS="1876=21877,1502=21502" ./spro run sim`. Подключения самого симулятора
к `127.0.0.1` на эти порты тоже перенаправляются; тесты `./spro test sim` так и работают.

Docker нужен для QEMU, i386 и проверки установки. Образы: `espressif/idf:v6.0.3`
(~10 ГБ), `sedonapro/i386-dev` (собирается из `emu/i386/Dockerfile`).

## Что поймала эмуляция

- **QEMU ESP32-S3:** `App.startApp` обходил `comps` через `foreach`, а `LogMgr.start()`
  добавляет компоненты и перевыделяет массив. Цикл читал освобождённую память:
  на ПК (glibc) это незаметно, на ESP32 эту память сразу занимает lwIP. Исправлено
  в `core/kits/sys/App.sedona`. Найдено точкой наблюдения gdb в QEMU.
- `esp_adc` калибрует АЦП в конструкторе и в QEMU зависает; для профиля QEMU
  АЦП отключён (`CONFIG_SEDONA_HAL_ADC=n`).

## Отладка прошивки в QEMU

```sh
# GDB-сервер QEMU на :1234, остановка до старта
docker run --rm -it --network host -v $PWD:$PWD -w $PWD/build/esp32/fw-qemu espressif/idf:v6.0.3 \
  qemu-system-xtensa -M esp32s3 -m 32M -drive file=qemu_flash.bin,if=mtd,format=raw \
  -drive file=qemu_efuse.bin,if=none,format=raw,id=efuse \
  -global driver=nvram.esp32s3.efuse,property=drive,value=efuse \
  -global driver=timer.esp32s3.timg,property=wdt_disable,value=true \
  -nic user,model=open_eth -nographic -gdb tcp::1234 -S
# в другом окне: xtensa-esp32s3-elf-gdb esp32-sedona.elf -ex "target remote :1234"
```

При ошибке VM печатает `unknown@0x<смещение>` для методов без отладочной информации
и `virtual call to method 0` с адресом объекта, если испорчен заголовок компонента.
