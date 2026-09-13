# Валидация и Проверки (VALIDATION)

Списък с реално извършените тестове и валидации върху хранилището и кода.

## Documentation
- [x] Internal Markdown links checked
- [x] Referenced files exist
- [x] No contradictory feature claims
- [x] Consistency of paths, versions (ESP-IDF 5.5.5, ESP32-P4)

## Firmware
- [ ] Clean build
- [ ] Flash
- [ ] Boot
- [ ] UI
- [ ] Touch

## USB
- [ ] Keyboard HID
- [ ] Consumer HID
- [ ] Raw HID

## Host Agent
- [ ] Connect
- [ ] Commands
- [ ] Telemetry
- [ ] Auto profile

## Config
- [ ] Load
- [ ] Fallback
- [ ] Upload
- [ ] Reboot/reload

## Repository
- [ ] Fresh clone reproducibility (NOT VERIFIED)
- [x] No secrets present in repository
- [x] No GPS EXIF in screenshots
- [x] .gitignore correctly ignores personal configs, venv, and build artifacts.

> [!NOTE]
> Тестовете, свързани с хардуера (Firmware, USB, Host Agent execution), изискват физическо устройство и тепърва трябва да бъдат потвърдени от собственика.
