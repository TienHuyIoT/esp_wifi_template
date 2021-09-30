@echo off
echo ========================================================
echo Flash script program esp32 [03_flash_esp32_efuse_fe_release]

set /p ComPort="Nhap Cong COM (1 - 100): "

::Securing Flash Encryption
echo Securing Flash Encryption

::Read summary efuse
echo Read summary efuse

espefuse.py -p COM%ComPort% summary

::Write protect efuse FLASH_CRYPT_CNT
echo Write protect efuse [FLASH_CRYPT_CNT]

espefuse.py --port COM%ComPort% write_protect_efuse FLASH_CRYPT_CNT

::Burn efuse DISABLE_DL_ENCRYPT
echo Burn efuse [DISABLE_DL_ENCRYPT]

espefuse.py --port COM%ComPort% burn_efuse DISABLE_DL_ENCRYPT

::Read summary efuse again
echo Read summary efuse agian

espefuse.py -p COM%ComPort% summary

echo Complete.
echo ========================================================
pause