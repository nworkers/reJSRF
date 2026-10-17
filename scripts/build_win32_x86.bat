@echo off
rem Runs build_win32_x86.ps1 without changing the PowerShell execution policy
rem (the policy blocks .ps1 files by default). Arguments pass through:
rem   scripts\build_win32_x86.bat                      Debug
rem   scripts\build_win32_x86.bat Release              Release
rem   scripts\build_win32_x86.bat -Configuration Release -Target rejsrf
setlocal
set "ARGS=%*"
if "%~1"=="" set "ARGS=-Configuration Debug"
if /I "%~1"=="Debug" set "ARGS=-Configuration %*"
if /I "%~1"=="Release" set "ARGS=-Configuration %*"
if /I "%~1"=="RelWithDebInfo" set "ARGS=-Configuration %*"
if /I "%~1"=="MinSizeRel" set "ARGS=-Configuration %*"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_win32_x86.ps1" %ARGS%
exit /b %ERRORLEVEL%
