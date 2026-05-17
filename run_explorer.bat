@echo off
cd /d "%~dp0"
echo Checking dependencies...
python -m pip install --quiet --disable-pip-version-check numpy pygame
if errorlevel 1 (
    echo Failed to install dependencies. Make sure Python is installed and on PATH.
    pause
    exit /b 1
)
python explorer.py
pause
