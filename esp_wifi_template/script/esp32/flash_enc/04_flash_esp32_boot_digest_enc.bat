@echo off
echo ========================================================
echo Flash script program ESP32 [04_flash_esp32_app_enc]

set otadata=.\boot_app0.bin
set bootloader=.\bootloader_qio_80m.bin
set partition=.\bin\partitions.bin
set app=.\bin\app.bin
set data=.\bin\littlefs.bin
set fek=.\key\my_flash_encryption_key.bin
set sbk=.\key\secure_bootloader_key.bin

set bootloader_enc=.\bootloader_qio_80m_enc.bin
set bootloader_digest=.\bootloader_qio_80m_digest.bin
set bootloader_digest_enc=.\bootloader_qio_80m_digest_enc.bin
set partition_enc=.\bin\partitions_enc.bin
set app_enc=.\bin\app_enc.bin

set /p ComPort="Nhap Cong COM (1 - 100): "

:: ========================================================
:: Gen bootloader digest
echo Gen bootloader digest

espsecure.py digest_secure_bootloader --keyfile %sbk% --output %bootloader_digest% %bootloader%
:: Gen bootloader digest
:: ========================================================

:: ========================================================
:: Begin Development Mode
::echo Development Mode

:: Program encrypt image
::echo Program encrypt image

::esptool.py --chip esp32 --port COM%ComPort% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect --encrypt 0x0 %bootloader_digest%
:: End Development Mode
:: ========================================================

:: ========================================================
:: Begin Release Mode
echo Release Mode

:: encrypt_flash_data image
echo encrypt_flash_data image

espsecure.py encrypt_flash_data --keyfile %fek% --address 0x0 -o %bootloader_digest_enc% %bootloader_digest%

:: Program encrypt image
echo Program encrypt image

esptool.py --chip esp32 --port COM%ComPort% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x0 %bootloader_digest_enc%
:: End Release Mode
:: ========================================================

del %bootloader_digest%
del %bootloader_digest_enc%

echo Complete.
echo ========================================================
pause