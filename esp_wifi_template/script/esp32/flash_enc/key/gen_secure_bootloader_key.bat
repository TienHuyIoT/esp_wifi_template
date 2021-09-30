@echo off

set key=.\secure_bootloader_key.bin

set /p Confirm="generate Key Y/(N): "

if %Confirm%==Y ( 
    echo "generate KEY"
    espsecure.py generate_signing_key secure_boot_signing_key.pem
    espsecure.py digest_private_key --keyfile secure_boot_signing_key.pem --keylen 256 %key%
) else (
    echo "Not generate KEY"
)

pause