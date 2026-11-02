@echo off
rem ============================================================================
rem  TeraTerm Console - esp_wifi_template   (double-click this file)
rem
rem  Starts the WinForms UI through wscript, so no console window is left
rem  behind.  If Windows Script Host is unavailable the launcher falls back to
rem  a hidden PowerShell start.
rem ============================================================================
setlocal

if not exist "%~dp0TeraTermConsole.ps1" goto :missing
if not exist "%~dp0TeraTermConsole.vbs" goto :missing

where wscript.exe >nul 2>nul
if errorlevel 1 goto :fallback

start "" wscript.exe "%~dp0TeraTermConsole.vbs"
exit /b 0

:fallback
start "" /min powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0TeraTermConsole.ps1"
exit /b 0

:missing
echo.
echo   TeraTermConsole.ps1 / TeraTermConsole.vbs are missing from:
echo     %~dp0
echo.
echo   Keep the whole script\tera_console folder together.
echo.
pause
exit /b 1
