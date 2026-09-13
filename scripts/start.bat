@echo off
REM Starts the Touch Deck host agent. Does not touch the firmware -
REM use deploy.bat first if Tab5 isn't running the app yet.
setlocal
set "AGENT_DIR=%~dp0..\host-agent"
set "PYTHON_EXE=%~dp0..\.venv\Scripts\python.exe"

REM tasklist filters BY window title but does not print it in the output
REM table, so match on the Image Name column (cmd.exe) instead.
tasklist /FI "WINDOWTITLE eq TouchDeckAgent" | findstr /I "cmd.exe" >NUL
if not errorlevel 1 (
    echo Touch Deck agent is already running.
    goto :end
)

if not exist "%AGENT_DIR%\config.toml" (
    echo No config.toml found - copying config.example.toml as a starting point.
    copy /Y "%AGENT_DIR%\config.example.toml" "%AGENT_DIR%\config.toml" >nul
)

if not exist "%PYTHON_EXE%" (
    echo Project Python environment is missing.
    echo Run: python -m venv .venv
    echo Then install: .venv\Scripts\python.exe -m pip install -r host-agent\requirements.txt
    goto :end
)

echo Starting Touch Deck host agent...
start "TouchDeckAgent" /MIN cmd /c ""%PYTHON_EXE%" "%AGENT_DIR%\agent.py" > "%AGENT_DIR%\agent.log" 2>&1"
echo Started. Log: %AGENT_DIR%\agent.log

:end
endlocal
pause
