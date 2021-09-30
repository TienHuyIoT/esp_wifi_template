@echo off
echo ========================================================
echo Flash script program esp32 [01_flash_esp32_efuse_burn_fek]

set fek=.\key\my_flash_encryption_key.bin

set /p ComPort="Nhap Cong COM (1 - 100): "

::Securing Flash Encryption
echo Securing Flash Encryption

::Read summary efuse
echo Read summary efuse

espefuse.py -p COM%ComPort% summary

::Burning Flash Encryption Key
echo Burning Flash Encryption Key

espefuse.py --port COM%ComPort% burn_key flash_encryption %fek%

::Read summary efuse again
echo Read summary efuse agian

espefuse.py -p COM%ComPort% summary

echo Complete.
echo ========================================================
pause