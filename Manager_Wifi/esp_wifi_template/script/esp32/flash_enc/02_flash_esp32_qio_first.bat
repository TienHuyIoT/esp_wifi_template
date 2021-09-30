@echo off
echo ========================================================
echo Flash script program esp32 [02_flash_esp32_qio_first]

set otadata=.\boot_app0.bin
set bootloader=.\bootloader_qio_80m.bin
set partition=.\bin\partitions.bin
set app=.\bin\app.bin
set data=.\bin\littlefs.bin
set /p ComPort="Nhap Cong COM (1 - 100): "

:: Program plaintext image
echo Program plaintext image

esptool.py --chip esp32 --port COM%ComPort% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0xe000 %otadata% 0x1000 %bootloader% 0x10000 %app% 0x9000 %partition% 0x310000 %data%
echo Complete.
echo ========================================================
pause