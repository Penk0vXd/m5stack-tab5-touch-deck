@echo off
REM Opens the dependency-free local layout editor and keeps its server in a
REM clearly named minimized window.
setlocal
set "ROOT_DIR=%~dp0.."
set "PYTHON_EXE=python"
if exist "%ROOT_DIR%\.venv\Scripts\python.exe" set "PYTHON_EXE=%ROOT_DIR%\.venv\Scripts\python.exe"

tasklist /FI "WINDOWTITLE eq TouchDeckEditorServer" | findstr /I "cmd.exe" >NUL
if errorlevel 1 (
    start "TouchDeckEditorServer" /MIN "%PYTHON_EXE%" -m http.server 8080 --bind 127.0.0.1 --directory "%ROOT_DIR%"
)

start "" "http://127.0.0.1:8080/editor/"
echo Touch Deck Studio opened in your browser.
echo Close the TouchDeckEditorServer window when you are finished.
endlocal
