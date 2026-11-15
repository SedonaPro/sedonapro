# Native kit ids

Every kit with native methods has a unique id (`<native qname=".." id="KIT::METHOD"/>`).
The VM finds natives by this number, so two kits in one image must never share it.

| Range | Owner |
|---|---|
| 0–99 | Sedona Framework core (Tridium) |
| 100–109 | reserved (tutorial = 101) |
| 110–199 | SedonaPro kits (table below) |
| 200–254 | community kits: take the next free id and add a row here in your pull request |

| Id | Kit | Was |
|---|---|---|
| 110 | sproMath | bclimateMath 106 |
| 111 | sproUtil | bclimateLibNative 104 |
| 112 | sproModbusMaster | bclimateModbusMaster 102 |
| 113 | sproModbusSlave | bclimateModbusSlave 103 |
| 114 | sproSpi | bclimateSpi 105 |
| 115 | sproSqlite | bclimateSqlite 108 |
| 116 | sproExpr | bclimateExpression 109 |
| 117 | sproMqtt | — (rewritten; was communityMQTT 50) |
| 120 | sproPlatformLinux | bclimatePlatform 107 |
| 121 | sproPlatformEsp32 | bclimateEsp32 110 |
| 122 | sproHal | — (new) |
