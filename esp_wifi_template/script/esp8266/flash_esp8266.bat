@echo off
echo ========================================================
echo Nap chuong trinh esp8266

set app=.\bin\app.bin
set data=.\bin\littlefs.bin
set /p ComGate="Nhap Cong COM (1 - 100): "

esptool.exe --chip esp8266 --port COM%ComGate% --baud 921600 --before default_reset --after hard_reset write_flash 0 %app% 0x200000 %data%
echo Complete.
echo ========================================================
pause