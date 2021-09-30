@echo off
echo ========================================================
echo Flash script program esp32 [01_flash_esp32_efuse_burn_sbk]

set sbk=.\key\secure_bootloader_key.bin

set /p ComPort="Nhap Cong COM (1 - 100): "

::secure bootloader enable
echo secure bootloader enable

::Read summary efuse
echo Read summary efuse

espefuse.py -p COM%ComPort% summary

::Burning secure bootloader Key
echo Burning secure bootloader Key

espefuse.py --port COM%ComPort% burn_key secure_boot_v1 %sbk%

::Read summary efuse again
echo Read summary efuse agian

espefuse.py -p COM%ComPort% summary

echo Complete.
echo ========================================================
pause