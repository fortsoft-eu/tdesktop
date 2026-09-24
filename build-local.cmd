@echo off
setlocal
if "%~1"=="" goto default
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-local.ps1" %*
exit /b %errorlevel%
:default
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-local.ps1" Build
set "result=%errorlevel%"
pause
exit /b %result%
