# Компилиране и Флашване (Build and Flash)

Този документ описва процедурите за компилиране, флашване и възстановяване на фърмуера.

## Normal build

От **ESP-IDF 5.5.5 PowerShell** терминал:
```powershell
cd firmware
idf.py set-target esp32p4
idf.py build
```

## Clean build

Ако имате проблеми с компилацията или кеширани файлове:
```powershell
idf.py fullclean
idf.py build
```

## Find serial ports

За да намерите правилния COM порт на Windows:
1. Отворете `Device Manager`.
2. Разгънете `Ports (COM & LPT)`.
3. Потърсете `USB Serial Device (COMx)`.

## Download mode

За да флашнете устройството (M5Stack Tab5), то трябва да е в "Download mode":
1. Натиснете и задръжте бутона **Reset**.
2. Задръжте го за около 2 секунди, докато зеленият LED започне да мига бързо.
3. Отпуснете бутона.

## Flash

След като сте в Download mode:
```powershell
idf.py -p COMx flash
```

## Monitor

Runtime фърмуерът няма USB CDC serial console по подразбиране. Ако използвате хардуерен UART за дебъгване, можете да използвате монитора:
```powershell
idf.py -p COMx monitor
```

## Erase flash

> [!CAUTION]
> Това е деструктивна операция! Ще изтрие всички конфигурации в SPIFFS.
```powershell
idf.py -p COMx erase-flash
```

## Factory recovery (Restore M5Stack Tab5 factory firmware)

Ако искате да върнете оригиналния фърмуер на M5Stack:
1. Изтеглете [M5Burner](https://m5stack.com/pages/download).
2. Свържете устройството.
3. Намерете Tab5 в софтуера и изберете "Burn".

## Failure recovery

- **flash fails**: Уверете се, че устройството е в Download mode (зеленият LED мига).
- **wrong COM**: Проверете отново Device Manager.
- **device not in download mode**: Повторете процедурата със задържане на Reset бутона за 2 секунди.
- **boot loop**: Обикновено причинено от проблем в конфигурацията. Направете `erase-flash` и флашнете наново.
- **black screen**: Възможно е грешен BSP; проверете дали сте сетнали правилно таргета.
- **corrupted SPIFFS**: Направете `erase-flash`.
