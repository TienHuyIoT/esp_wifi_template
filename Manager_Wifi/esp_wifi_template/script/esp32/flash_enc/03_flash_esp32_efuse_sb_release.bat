@echo off
echo ========================================================
echo Flash script program esp32 [03_flash_esp32_efuse_sb_release]

set /p ComPort="Nhap Cong COM (1 - 100): "

::Secure boot enable
echo Secure boot enable

::Read summary efuse
echo Read summary efuse

espefuse.py -p COM%ComPort% summary

::Write burn efuse ABS_DONE_0
echo burn efuse [ABS_DONE_0]

espefuse.py --port COM%ComPort% burn_efuse ABS_DONE_0

::Read summary efuse again
echo Read summary efuse agian

espefuse.py -p COM%ComPort% summary

echo Complete.
echo ========================================================
pause