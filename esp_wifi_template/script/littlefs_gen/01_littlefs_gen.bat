@echo off
echo ========================================================
echo littlefs data esp32

set src_folder=..\..\data\
set image_file=littlfs_gen.bin
set adr_offset=0x290000
set image_size=0x170000

mklittlefs.exe -c %src_folder% -d 0 -b 4096 -p 256 -s %image_size% %image_file%
echo Complete.
echo ========================================================
pause