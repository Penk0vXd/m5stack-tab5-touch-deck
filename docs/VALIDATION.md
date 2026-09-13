# Валидация и Проверки (VALIDATION)

Списък с реално извършените тестове и валидации върху хранилището и кода.

## Documentation
- [x] Internal Markdown links checked
- [x] Referenced files exist
- [x] No contradictory feature claims
- [x] Consistency of paths, versions (ESP-IDF 5.5.5, ESP32-P4)

## Firmware
- [x] Clean ESP-IDF 5.5.5 build (`touchdeck.bin`, 2026-09-13)
- [ ] Flash
- [ ] Boot
- [ ] UI
- [ ] Touch

## USB
- [ ] Keyboard HID
- [ ] Consumer HID
- [ ] Raw HID

## Host Agent
- [x] Protocol, command-worker and detached-command unit tests (6 tests)
- [x] Local `run_tests` and `git_status` command configuration returns `ok`
- [ ] Connect
- [ ] Commands
- [ ] Telemetry
- [ ] Auto profile

## Config
- [x] JSON syntax plus UI v2 grid, action, macro, tile and page-target validation
- [ ] Load
- [ ] Fallback
- [ ] Upload
- [ ] Reboot/reload

## Repository
- [ ] Fresh clone reproducibility (NOT VERIFIED)
- [x] No secrets present in repository
- [x] No GPS EXIF in screenshots
- [x] .gitignore correctly ignores personal configs, venv, and build artifacts.

## Visual Editor
- [x] Correct URL/title and non-blank DOM
- [x] Home preview and four-page navigation rendered
- [x] Inspector selection and live label update
- [x] Invalid action produces a visible validation error
- [x] No browser console warnings/errors during smoke test

> [!NOTE]
> Тестовете, свързани с хардуера (Firmware, USB, Host Agent execution), изискват физическо устройство и тепърва трябва да бъдат потвърдени от собственика.
