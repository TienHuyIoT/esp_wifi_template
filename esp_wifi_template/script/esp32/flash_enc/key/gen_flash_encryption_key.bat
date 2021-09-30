@echo off

set key=.\my_flash_encryption_key_1.bin

set /p Confirm="generate Key Y/(N): "

if %Confirm%==Y ( 
    echo "generate KEY"
    espsecure.py generate_flash_encryption_key %key%
) else (
    echo "Not generate KEY"
)

pause