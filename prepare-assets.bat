@echo off
setlocal
cd /d "%~dp0"
if /i "%~1"=="--inspect-target" goto readonly
if /i "%~1"=="--verify-only" goto readonly
if not exist ".setup-venv\Scripts\python.exe" (
  py -3 -m venv .setup-venv
  if errorlevel 1 goto failed
)
".setup-venv\Scripts\python.exe" -c "import zstandard" >nul 2>&1
if errorlevel 1 (
  ".setup-venv\Scripts\python.exe" -m pip install -r requirements.txt
  if errorlevel 1 goto failed
)
".setup-venv\Scripts\python.exe" -B -X utf8 tools\prepare_assets.py %*
exit /b %errorlevel%
:readonly
py -3 -B -X utf8 tools\prepare_assets.py %*
exit /b %errorlevel%
:failed
echo Setup failed. Install Python 3.12 or newer with the Python launcher.
exit /b 1
