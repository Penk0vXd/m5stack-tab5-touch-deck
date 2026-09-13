@echo off
REM Builds and flashes the current Touch Deck firmware. Runtime HID presence is
REM informational only: it must never cause an upgrade to be skipped.
setlocal enabledelayedexpansion
set "FW_DIR=%~dp0..\firmware"
set "IDF_TOOLS_PATH=C:\Espressif"
set "IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.5"

REM Prefer an installed Python environment that matches ESP-IDF 5.5. This also
REM avoids a stale launcher pointing at a removed Python version.
for /d %%E in ("%IDF_TOOLS_PATH%\python_env\idf5.5_py*_env") do (
    if exist "%%~fE\Scripts\python.exe" set "IDF_PYTHON_ENV_PATH=%%~fE"
)

echo Checking whether Tab5 is running the Touch Deck firmware...
for /f "usebackq delims=" %%R in (`powershell -NoProfile -Command "if (Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -match 'VID_303A.*PID_4004' }) { 'yes' } else { 'no' }"`) do set "RUNNING=%%R"

if /I "!RUNNING!"=="yes" echo Runtime firmware detected. Put Tab5 in download mode before continuing.
if /I not "!RUNNING!"=="yes" echo Runtime HID is not present. Put Tab5 in download mode before continuing.

if not exist "%IDF_PATH%\export.bat" (
    echo ESP-IDF not found at %IDF_PATH%
    echo Edit IDF_PATH/IDF_TOOLS_PATH at the top of this script if it was installed elsewhere.
    goto :end
)

call "%IDF_PATH%\export.bat"
if errorlevel 1 goto :end

pushd "%FW_DIR%"
call idf.py build
if errorlevel 1 (
    echo Build failed - flash was not attempted.
    popd
    goto :end
)

echo Available serial ports:
python -m serial.tools.list_ports -v
set /p PORT="Download port (for example COM3): "
if "!PORT!"=="" (
    echo No port entered, aborting.
    popd
    goto :end
)

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
