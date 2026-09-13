# Инсталация и Настройка (Setup)

Това ръководство описва стъпките за подготовка на чист компютър (Windows) за разработка и използване на Tab5 Touch Deck.

> [!WARNING]
> Fresh-clone reproducibility has not yet been fully validated.
> See CURRENT_STATE.md for known limitations.

## 1. Git

Инсталирайте [Git за Windows](https://git-scm.com/download/win).

Проверка на инсталацията:
```powershell
git --version
```

## 2. ESP-IDF

Инсталирайте **ESP-IDF 5.5.5**, като следвате [официалното ръководство](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32/get-started/windows-setup.html). (Използвайте v5.3+ документацията като най-близка, но инсталирайте точно версия 5.5.5).

Точната потвърдена версия:
```text
ESP-IDF 5.5.5
Target: ESP32-P4
```

Проверка на инсталацията (в ESP-IDF PowerShell терминал):
```powershell
idf.py --version
```

## 3. Python

Инсталирайте [Python 3.11 или по-нова версия](https://www.python.org/downloads/). Уверете се, че сте избрали опцията "Add Python to PATH".

Проверка на версията:
```powershell
python --version
```

## 4. Клониране на хранилището (Repository clone)

Отворете PowerShell и изпълнете:
```powershell
git clone --recurse-submodules REPLACE_WITH_REPOSITORY_URL tab5-touch-deck
cd tab5-touch-deck
```

*(Забележка: `REPLACE_WITH_REPOSITORY_URL` трябва да се замени с реалния URL на хранилището, след като то бъде публикувано.)*

## 5. Virtual Environment & Python Dependencies

Създайте виртуална среда и инсталирайте зависимостите за host agent:
```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r host-agent\requirements.txt
```

## 6. Dependency verification

Уверете се, че BSP и ESP-IDF зависимостите са налични в `firmware` папката (ще се изтеглят автоматично при първи build).

## 7. Final setup verification

Използвайте следния checklist, за да сте сигурни, че всичко е наред:
```text
[ ] git works
[ ] ESP-IDF 5.5.5 active
[ ] esp32p4 target available
[ ] Python environment active
[ ] host dependencies installed
[ ] firmware configure succeeds
```
