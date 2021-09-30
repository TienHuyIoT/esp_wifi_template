@echo off
echo ========================================================
echo Flash script program ESP32 [04_flash_esp32_app_enc]

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

:: ========================================================
:: Begin Development Mode
echo Development Mode

:: Program encrypt image
echo Program encrypt image

esptool.py --chip esp32 --port COM%ComPort% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect --encrypt 0x10000 %app%
:: End Development Mode
:: ========================================================

:: ========================================================
:: Begin Release Mode
::echo Release Mode

:: encrypt_flash_data image
::echo encrypt_flash_data image

::espsecure.py encrypt_flash_data --keyfile %key% --address 0x10000 -o %app_enc% %app%

:: Program encrypt image
::echo Program encrypt image

::esptool.py --chip esp32 --port COM%ComPort% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x10000 %app_enc%
:: End Release Mode
:: ========================================================

::del %app_enc%

echo Complete.
echo ========================================================
pause