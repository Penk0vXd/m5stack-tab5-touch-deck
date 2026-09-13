@echo off
REM Checks whether Tab5 is already running the Touch Deck firmware
REM (looks for the composite USB device it enumerates as, VID 303A / PID
REM 4004). If it's not there, asks which COM port Tab5 is on and flashes it.
setlocal enabledelayedexpansion
set "FW_DIR=%~dp0..\firmware"
set "IDF_TOOLS_PATH=C:\Espressif"
set "IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.5"

echo Checking whether Tab5 is running the Touch Deck firmware...
for /f "usebackq delims=" %%R in (`powershell -NoProfile -Command "if (Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -match 'VID_303A.*PID_4004' }) { 'yes' } else { 'no' }"`) do set "RUNNING=%%R"

if /I "!RUNNING!"=="yes" (
    echo Touch Deck firmware is already running on Tab5. Nothing to flash.
    goto :end
)

echo Touch Deck firmware was not detected on Tab5.
set /p PORT="Which COM port is Tab5 connected to (e.g. COM3): "
if "!PORT!"=="" (
    echo No port entered, aborting.
    goto :end
)

if not exist "%IDF_PATH%\export.bat" (
    echo ESP-IDF not found at %IDF_PATH%
    echo Edit IDF_PATH/IDF_TOOLS_PATH at the top of this script if it was installed elsewhere.
    goto :end
)

call "%IDF_PATH%\export.bat"
if errorlevel 1 goto :end

pushd "%FW_DIR%"
echo Flashing firmware to !PORT! ...
call idf.py -p !PORT! flash
set "FLASH_RESULT=!errorlevel!"
popd

if "!FLASH_RESULT!"=="0" (
    echo Flash complete. Tab5 is rebooting into the new firmware.
) else (
    echo Flash failed - see the output above.
)

:end
endlocal
pause
