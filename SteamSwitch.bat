@echo off
REM Double-click launcher for SteamSwitch (pywebview UI).
REM Checks for the pywebview package on first run and offers to install it.
cd /d "%~dp0"

set "PYW="
set "PY="
where pythonw >nul 2>nul && set "PYW=pythonw"
where python  >nul 2>nul && set "PY=python"
if not defined PY if not defined PYW goto :nopython

REM Use a console-attached interpreter for the dependency check/install.
set "CHECK=%PY%"
if not defined CHECK set "CHECK=%PYW%"

%CHECK% -c "import webview" >nul 2>nul
if errorlevel 1 goto :needdep
goto :launch

:needdep
echo.
echo SteamSwitch's window needs the "pywebview" package (one-time setup).
set /p "ANS=Install it now with pip? [Y/n] "
if /I "%ANS%"=="N" goto :manual
echo Installing... this can take a minute.
%CHECK% -m pip install -r "%~dp0requirements.txt"
if errorlevel 1 goto :failed
goto :launch

:launch
if defined PYW (
    start "" pythonw "%~dp0webapp.py"
) else (
    start "" %PY% "%~dp0webapp.py"
)
exit /b

:manual
echo.
echo No problem. Install it yourself with:
echo     pip install -r requirements.txt
echo.
pause
exit /b

:failed
echo.
echo The install failed. Try it manually:
echo     pip install -r requirements.txt
echo.
pause
exit /b

:nopython
echo Python was not found on PATH.
echo Install Python 3 from https://www.python.org/downloads/ and tick
echo "Add python.exe to PATH" during setup, then double-click this file again.
pause
exit /b
