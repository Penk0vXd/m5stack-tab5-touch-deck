@echo off
REM Stops the Touch Deck host agent. Firmware on Tab5 keeps running either way -
REM this only stops the PC-side agent.py process.
setlocal
echo Stopping Touch Deck agent...

taskkill /FI "WINDOWTITLE eq TouchDeckAgent" /T /F >NUL 2>&1
if errorlevel 1 (
    echo No agent window found under that title.
) else (
    echo Stopped agent window.
)

endlocal
pause
