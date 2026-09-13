# Документация на Tab5 Touch Deck

[Начална страница](../README.md)

Snapshot: **2026-09-13**. Източник на истината са application кодът, локалният BSP, manifests и конфигурациите. Старите логове са историческо доказателство, а не нов хардуерен тест. Имената на API, команди и UI остават както са в проекта.

## Път за първо стартиране

1. [Цел](PROJECT_OVERVIEW.md) и [реално състояние](CURRENT_STATE.md).
2. [Хардуер](HARDWARE.md) и [software stack](SOFTWARE.md).
3. [Среда от чист компютър](SETUP.md).
4. [Конфигурация](CONFIGURATION.md).
5. [Build и flash](BUILD_AND_FLASH.md).
6. [Употреба](USAGE.md) и [диагностика](TROUBLESHOOTING.md).

## Път за разработчик

[Структура](PROJECT_STRUCTURE.md) → [Архитектура](ARCHITECTURE.md) → [Development](DEVELOPMENT.md) → [Security](SECURITY.md) → [Validation](VALIDATION.md).

## Поддръжка

- [IMAGES](IMAGES.md): снимки и изисквания към изображенията.
- [ROADMAP](ROADMAP.md): предложения, които още не са доставени.
- [CHANGELOG](CHANGELOG.md): промени без измислени releases.
- [TAB5_PROJECT_IDEAS](TAB5_PROJECT_IDEAS.md): оригинален каталог; Touch Deck е идея 04. Другите идеи не са функции на firmware-а. Общите хардуерни описания там не заменят текущите BSP и HARDWARE.

`TODO` е предстояща задача; `Needs verification` е непотвърдено поведение; `Not implemented yet` е липсваща реализация. При промяна на поведение обновявайте ръководството, CURRENT_STATE и CHANGELOG. Активирайте image reference само след добавяне на действителния файл. Не публикувайте локални настройки или необработени build логове.
