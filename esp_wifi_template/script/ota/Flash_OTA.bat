@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "DEFIP=192.168.3.102"
set "IP=%DEFIP%"
set "OPTS="

echo ========================================================
echo  esp_wifi_template - Nap firmware qua IP (OTA / espota)
echo ========================================================
echo.

set /p "IN=Nhap IP board [%DEFIP%]: "
if not "%IN%"=="" set "IP=%IN%"
set "IN="

set /p "B=Build lai truoc khi OTA? (y/N): "
if /i "%B%"=="y" set "OPTS=%OPTS% -Build"
set "B="

set /p "D=Nap ca LittleFS? (y/N): "
if /i "%D%"=="y" set "OPTS=%OPTS% -Target littlefs"
set "D="
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Flash-OTA.ps1" -Ip %IP% %OPTS% %*

echo.
if errorlevel 1 (
    echo ========================================================
    echo  THAT BAI
    echo ========================================================
) else (
    echo ========================================================
    echo  HOAN TAT
    echo ========================================================
)
pause
