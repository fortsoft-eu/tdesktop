@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-local.ps1" Run
set "result=%errorlevel%"
if not "%result%"=="0" pause
exit /b %result%
