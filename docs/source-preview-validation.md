# Source preview validation

Validation performed on 15 November 2026 in an isolated Debian 12 amd64 build
directory, using the staged publication sources. Running bench services were
not changed.

| Check | Result |
| --- | --- |
| `./spro build linux` | Passed, Pro profile, simulated board |
| `./spro test linux --quick` | Passed: platform, HTTP, SOX widget archive, path rejection and virtual digital/analogue I/O |
| `./spro catalog --check` | Passed after regenerating the previously outdated catalog |
| `./spro ui check` | Passed |
| `python3 tools/tests/ui_check_test.py` | Passed, zero failures |
| Offline VM tests | Four images, 126,101 successful assertions, zero failed assertions |
| `node --test editor/tests/*.test.mjs` | 42 passed, zero failed |

The offline run excludes the legacy `inet` and `serial` unit-test suites. One
legacy network test waits for a connection to an unavailable external host;
serial tests need their own environment. Those suites are not counted as passed.
The offline runner is available as `python3 tools/test_offline.py` after building.

These checks do not establish real-board electrical I/O acceptance, full
ESP32/QEMU firmware validation, clean builds on every host, browser end-to-end
acceptance or interoperability with all third-party Sedona clients. The source
preview does not include a signed installer or a production firmware release.
