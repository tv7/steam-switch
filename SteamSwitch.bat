@echo off
REM Double-click launcher for SteamSwitch (pywebview UI).
REM First run needs:  pip install -r requirements.txt
REM Runs webapp.py with no console window (pythonw), falling back to python.
REM The dependency-free Tkinter UI is still available via SteamSwitch-classic.bat.
cd /d "%~dp0"
where pythonw >nul 2>nul
if %errorlevel%==0 (
    start "" pythonw "%~dp0webapp.py"
) else (
    where python >nul 2>nul
    if %errorlevel%==0 (
        start "" python "%~dp0webapp.py"
    ) else (
        echo Python was not found on PATH.
        echo Install Python 3 from https://www.python.org/downloads/ and tick
        echo "Add python.exe to PATH" during setup, then double-click this file again.
        pause
    )
)
