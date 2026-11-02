@echo off
echo ========================================================
echo littlefs data esp32

set src_folder=..\..\data\
set image_file=littlfs_gen.bin
set adr_offset=0x290000
set image_size=0x170000
set /p ComGate="Nhap Cong COM (1 - 100): "

..\esptool.exe --chip esp32 --port COM%ComGate% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect %adr_offset% %image_file%
echo Complete.
echo ========================================================
pause