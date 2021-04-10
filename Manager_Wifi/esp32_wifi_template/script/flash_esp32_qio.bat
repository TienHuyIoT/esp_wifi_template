@echo off
echo ========================================================
echo Nap chuong trinh esp32

set boot_app=.\boot_app0.bin
set bootloader=.\bootloader_qio_80m.bin
set partition=.\bin\partitions.bin
set app=.\bin\app.bin
set data=.\bin\littlefs.bin
set /p ComGate="Nhap Cong COM (1 - 100): "

esptool.exe --chip esp32 --port COM%ComGate% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0xe000 %boot_app% 0x1000 %bootloader% 0x10000 %app% 0x8000 %partition% 0x290000 %data%
echo Complete.
echo ========================================================
pause