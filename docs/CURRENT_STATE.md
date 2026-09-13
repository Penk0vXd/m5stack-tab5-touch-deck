# Текущо състояние

[Индекс](README.md) · [Проверки](VALIDATION.md) · [Security](SECURITY.md)

Snapshot: **2026-09-13**. Няма `.git`, commit SHA или история в предоставената папка. „Working“ означава реализирано и подкрепено от посоченото доказателство; не означава пълен acceptance test на настоящия хардуер.

## Working

| Функция | Доказателство и граница |
|---|---|
| Build | Исторически `firmware/build_log.txt`: `Project build complete`; новата проверка е във VALIDATION |
| Boot до UI | `monitor_boot3.log`: Home с 15 бутона, 80% backlight, TinyUSB installed, BMI270 initialized, `touchdeck: ready` |
| Host HID discovery | `host-agent/agent.log`: `connected to M5Stack Touch Deck Control` |
| CMD достига агента | `agent_live_err.log`: получен `git_status`; изпълнение с грешка за несъществуващ `cwd` |
| Страници и widgets | Parser/UI и доставени Home/Git/Media; необходим е повторен хардуерен smoke test |

## Partially Working

| Функция | Реализация | Ограничение |
|---|---|---|
| Shortcuts/text/consumer | HID queue и press/release | ASCII/US layout; някои queue errors се игнорират |
| Macro | До 8 стъпки | Delay подрежда само HID queue; agent/goto не чакат HID |
| Config upload | Temp, parse, replace, reload | Неатомарен replace; без chunk ACK/retry/checksum; ACK преди UI reload |
| Fallback | Вграден deck | JSON error path има use-after-free; OOM/BSP assert могат да спрат boot |
| Auto profile | Window title matching | Само Windows; 61 UTF-8 bytes; граничен memory bug |
| Telemetry | CPU/mem/disk | Стойностите остават при offline, без timestamp |
| Volume slider | Относителни стъпки | Започва на 50, не отчита реалната host сила на звука |
| Long press | Отделно действие и `+` | Click при отпускане не е потиснат |
| IMU wake/dim | Tilt, movement и 8% dim | Wake не нулира LVGL inactivity |
| Audio/IMU fallback | Application обработва error | BSP assert може да се случи преди връщане на error |
| Windows scripts | Start/stop/deploy | Твърди пътища, широк stop filter, deploy пропуска наличен VID/PID |

## Not Implemented

`Not implemented yet`: Settings screen, главно menu screen, UI config editor, icons renderer, swipe navigation, Wi-Fi/BLE/C6 integration, camera/mic app, SD loading, battery UI, HTTP API, OTA/rollback, подписани конфигурации, CI/test suite и release процедура. Disk tile е поддържан в код, но не е поставен в доставените страници. Няма root application license.

## Known Issues

Пътищата за C модулите са относителни към `firmware/main/`.

| ID | Място | Проблем / препоръка |
|---|---|---|
| K01 | `cfg/config.c`, `td_config_load` | `free(raw)` преди `cJSON_GetErrorPtr()` при invalid JSON: use-after-free |
| K02 | `usb/raw_hid.c`, PROFILE | 61-byte буфер се запълва с 61 bytes без NUL; callback може да чете извън него |
| K03 | `cfg/config_rx.c`, `handle_end` | `unlink` преди `rename`: прекъсване/failure губи стария config; пазете host backup |
| K04 | `cfg/config_rx.c` / `agent.py` | Queue result не се проверява; няма upload size bound при запис, session timeout, sequence validation или retry; final ACK seq е 0 |
| K05 | `ui/deck_ui.c`, reload | ACK преди callback; lock/parse failure не стига host; повторен reload пише в активния модел |
| K06 | `cfg/config.c`, slider | `step` се cast-ва към uint8; 256 става 0 и може да причини деление на нула. Използвайте 1–100 |
| K07 | `ui/deck_ui.c` | Long press, stale telemetry, wake/inactivity и игнорирани HID errors — виж таблицата по-горе |
| K08 | `raw_hid.c` / `agent.py` | И двете страни отговарят PING на PING; външен ping може да започне непрекъснат обмен |
| K09 | `agent.py`, `serve` | `link.open()` е извън reconnect try; OSError може да спре агента; командите блокират receive loop |
| K10 | `scripts/stop.bat` | Спира всички `python.exe` с `agent.py` в command line, включително чужди проекти |
| K11 | `host-agent/config.toml` | Примерният `cwd` липсва; исторически WinError 267. Notes чака затваряне и може да timeout-не след 30 s |
| K12 | `firmware/dependencies.lock` | Личен абсолютен BSP path; ignored. Няма portable lock за fresh clone |
| K13 | `usb/raw_hid.c` | HELLO се опитва веднъж, често преди mount; „sent“ не доказва работещ агент; няма raw endpoint retry |
| K14 | `ui/deck_ui.c` | Споделени ACK/profile strings без пълно заключване; atomic counter не изключва едновременен следващ запис |

Наблюдения от кода, без exploit тестове. Source файловете не са променяни в документационната задача.

## Needs Testing

- Fresh clone на друг компютър без наличен dependency lock/cache.
- Flash и USB enumeration на конкретната ревизия; UART по проверена схема.
- Всички 25 доставени widgets, keyboard layout, press/release и media behavior.
- Upload, втори upload, invalid JSON, прекъсване, low space и reboot.
- CPU/RAM сравнение, auto profile и 61-byte title след поправка K02.
- Long press, reconnect, макроси под товар, dim/IMU wake.
- GT911/ST7123/ST7121 display/touch, audio и Linux/macOS.
