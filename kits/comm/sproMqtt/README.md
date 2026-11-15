# sproMqtt

Клиент MQTT 3.1.1 для Linux-контроллеров и ESP32: публикация и подписка точек,
JSON, статус online/offline. Свой клиент на C без потоков и сторонних библиотек:
VM опрашивает его в каждом цикле, сокеты неблокирующие.

## Как собрать приложение

```
MqttClient  host, port, username, password, baseTopic = "site/ahu1"
├── MqttPubFloat   topic "~/temp"        in ← датчик          → site/ahu1/temp  21.5
├── MqttPubJson    topic "~/state"       in1..in8, keys "t,sp" → site/ahu1/state {"t":21.5,"sp":22}
├── MqttSubFloat   topic "~/cmd/sp"      out → уставка        ← site/ahu1/cmd/sp 22
└── MqttSubFloat   topic "zigbee2mqtt/room"  jsonPath "temperature"
```

Точки — дочерние компоненты `MqttClient`. Топик, начинающийся с `~`, считается от
`baseTopic`. С `statusTopic` (по умолчанию включён) клиент публикует `online` в
`<baseTopic>/status` и оставляет брокеру `offline` как завещание (LWT): при обрыве
связи или пропаже питания брокер сам сообщит, что контроллер недоступен.

## Компоненты

| Компонент | Главные слоты | Что делает |
|---|---|---|
| `MqttClient` | `host`, `port`, `clientId`, `username`, `password`, `keepAlive`, `baseTopic`, `statusTopic`, `enabled`; `state`, `lastError`, `sent`, `received`, `connects` | соединение с брокером, переподключение через 1…30 с |
| `MqttPubBool` | `in`, `onText`/`offText` | `true`/`false` (для Home Assistant — `ON`/`OFF`) |
| `MqttPubInt` | `in` | целое числом |
| `MqttPubFloat` | `in`, `decimals`, `deadband` | число; изменения меньше `deadband` не отправляются |
| `MqttPubStr` | `in` | строка |
| `MqttPubJson` | `in1`…`in8`, `keys`, `decimals`, `deadband` | до 8 чисел одним JSON-объектом |
| `MqttSubBool` | `out`, `initial`, `jsonPath`, `staleTime` | `true/false`, `on/off`, `yes/no`, числа |
| `MqttSubInt`, `MqttSubFloat`, `MqttSubStr` | `out`, `initial`, `jsonPath`, `staleTime` | значение из сообщения или поле JSON (`a.b.0`) |

`initial` — значение `out` до первого сообщения (уставка не обнулится, пока брокер
не ответил). У всех публикаций: `topic`, `qos` (0/1), `retain`, `minInterval` (не чаще), `maxInterval`
(повторять без изменений, 0 — нет), действие `send`. После переподключения текущее
значение отправляется снова.

`status` точки: `noClient` (нет родителя `MqttClient`), `offline`, `ok`, `busy` (буфер
занят, повторит), `waiting` (подписка ждёт сообщения), `stale` (нет сообщений дольше
`staleTime`), `badValue` (не число / нет поля JSON), `tooBig`, `badTopic`.

`lastError` клиента: `noHost`, `dns`, `connect` (брокер недоступен), `timeout`,
`protocol`, `auth` (неверный логин/пароль), `rejected`, `closed`, `noMemory`.

## Ограничения

- MQTT 3.1.1, QoS 0 и 1, без TLS (следующий этап).
- Сообщение до 1 КБ, хранится до 512 байт на подписку; до 32 подписок и 8
  неподтверждённых QoS 1 сообщений на клиент.
- Имя хоста разрешается DNS синхронно при подключении; IP-адрес — без задержки.
- `clientId` по умолчанию: `spro-<MAC>` на ESP32, `spro-<хост>-<pid>` на Linux.

## Проверка

`./spro test sim` (с `sproMqtt` в образе) поднимает тестовый брокер
`tools/mqtt_broker.py` и проверяет пароль, LWT, публикацию,
подписку, JSON и «пропажу питания»; `./spro test esp32` — то же для прошивки в QEMU.
Брокер для своих опытов: `./spro broker`, сообщение ему: `./spro pub ТОПИК ЗНАЧЕНИЕ`;
готовый пример — `./spro build sim --example mqtt` (`examples/mqtt.xml`).

Номер нативного kit: 117. Лицензия: Apache 2.0.
