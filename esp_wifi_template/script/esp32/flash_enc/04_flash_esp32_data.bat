@echo off
echo ========================================================
echo Flash script program esp32 [04_flash_esp32_data]

set otadata=.\boot_app0.bin
set bootloader=.\bootloader_qio_80m.bin
set partition=.\bin\partitions.bin
set app=.\bin\app.bin
set data=.\bin\littlefs.bin
set key=.\key\my_flash_encryption_key.bin

set bootloader_enc=.\bootloader_qio_80m_enc.bin
set partition_enc=.\bin\partitions_enc.bin
set app_enc=.\bin\app_enc.bin

set /p ComPort="Nhap Cong COM (1 - 100): "

:: Program plaintext image
echo Program plaintext image

esptool.py --chip esp32 --port COM%ComPort% --baud 921600 --before default_reset --after no_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x310000 %data%

echo Complete.
echo ========================================================
pause