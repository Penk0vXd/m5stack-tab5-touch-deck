# Текущо състояние

[Индекс](README.md) · [Проверки](VALIDATION.md) · [Security](SECURITY.md)

Snapshot: **2026-09-14**, branch `main`, base commit `dbd8804`. „Working“ означава реализирано и подкрепено от посоченото доказателство; не означава пълен acceptance test на настоящия хардуер.

## Working

| Функция | Доказателство и граница |
|---|---|
| Build | ESP-IDF 5.5.5 regression build на 2026-09-13: `Project build complete`, `touchdeck.bin` 0x114030 bytes, 78% app partition free |
| Boot до UI | `monitor_boot3.log`: Home с 15 бутона, 80% backlight, TinyUSB installed, BMI270 initialized, `touchdeck: ready` |
| Host HID discovery | `host-agent/agent.log`: `connected to M5Stack Touch Deck Control` |
| CMD достига агента | `agent_live_err.log`: получен `git_status`; изпълнение с грешка за несъществуващ `cwd` |
| Local command config | Реалните `run_tests` и `git_status` са изпълнени през `CommandRunner` с status `ok`; GUI действията са покрити с mocked detached-command test |
| UI v2 | 12×4 asymmetric layouts, spans, icons, hints, variants, Home/Work/Media/System и bottom navigation; build verified, нужен е hardware smoke test |
| Visual editor | Локален preview/Inspector, drag-and-drop, add/duplicate/delete, layout validation, JSON open/export; browser smoke test без console errors |

## Partially Working

| Функция | Реализация | Ограничение |
|---|---|---|
| Shortcuts/text/consumer | HID queue и press/release; queue errors се показват в UI | ASCII/US layout |
| Macro | До 8 стъпки | Delay подрежда само HID queue; agent/goto не чакат HID |
| Config upload | Temp, parse, sequence/size/timeout validation, backup, replace, UI reload и rollback | Без chunk ACK/retry/checksum; хардуерен interruption test предстои |
| Fallback | Вграден deck | JSON error path е безопасен; OOM/BSP assert могат да спрат boot |
| Auto profile | Window title matching | Само Windows; 61 UTF-8 bytes се терминират безопасно |
| Telemetry | CPU/mem/disk със stale timeout | Няма history или host timestamp |
| Volume slider | Относителни стъпки | Започва на 50, не отчита реалната host сила на звука |
| Long press | Отделно действие и `+` | Click при отпускане вече се потиска; нужен е touch smoke test |
| IMU wake/dim | Tilt, movement, 8% dim и LVGL activity reset | Нужен е hardware tuning |
| Audio/IMU fallback | Application обработва error | BSP assert може да се случи преди връщане на error |
| Windows scripts | Start/stop/deploy/editor | Deploy build-ва и не пропуска upgrade при наличен runtime HID; остава твърд ESP-IDF path и ръчен download port |

## Not Implemented

`Not implemented yet`: touch swipe navigation, Wi-Fi/BLE/C6 integration, camera/mic app, SD loading, battery UI, HTTP API, OTA/rollback, подписани конфигурации, CI и release процедура. Няма root application license.

## Known Issues

Пътищата за C модулите са относителни към `firmware/main/`.

| ID | Място | Проблем / препоръка |
|---|---|---|
| K04 | `cfg/config_rx.c` / `agent.py` | Има size bound, timeout, sequence validation, queue error и правилен final ACK; липсват chunk ACK/retry/checksum |
| K12 | `firmware/dependencies.lock` | Личен абсолютен BSP path; ignored. Няма portable lock за fresh clone |

Поправени в текущото работно дърво: K01, K02, K03, K05, K06, K07, K08, K09, K10, K11 и K14. Промените минават firmware build, 6 host unit tests и browser smoke test, но все още не са flash-нати и проверени на хардуер.

Наблюденията за оставащите проблеми са от code review, без exploit тестове.

## Needs Testing

- Fresh clone на друг компютър без наличен dependency lock/cache.
- Flash и USB enumeration на конкретната ревизия; UART по проверена схема.
- Всички доставени UI v2 widgets, bottom navigation, keyboard layout, press/release, brightness и media behavior.
- Upload, втори upload, invalid JSON, прекъсване, low space и reboot.
- CPU/RAM сравнение, auto profile и 61-byte title след поправка K02.
- Long press, reconnect, макроси под товар, dim/IMU wake.
- GT911/ST7123/ST7121 display/touch, audio и Linux/macOS.
