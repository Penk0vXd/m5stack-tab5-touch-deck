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

REM Fallback: catch any agent.py process the window-title kill missed
REM (e.g. started manually outside start.bat).
powershell -NoProfile -Command "Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'python.exe' -and $_.CommandLine -like '*agent.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force; Write-Output ('Killed stray agent.py process ' + $_.ProcessId) }"

endlocal
pause
