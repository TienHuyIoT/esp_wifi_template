#ifndef	_APP_CONFIG_
#define _APP_CONFIG_

//==========================================================
// <o> FW_VERSION_MAJOR
 
#ifndef FW_VERSION_MAJOR
#define FW_VERSION_MAJOR 1
#endif

//==========================================================
// <o> FW_VERSION_MINOR
 
#ifndef FW_VERSION_MINOR
#define FW_VERSION_MINOR 0
#endif

//==========================================================
// <o> FW_VERSION_BUILD
 
#ifndef FW_VERSION_BUILD
#define FW_VERSION_BUILD 0
#endif

/* DNS Server
0: Disable
1: Enable
*/
#define DNS_SERVER_ENABLE  1

/* Enable sd card
0: Disable
1: Enable
*/
#define SD_CARD_ENABLE  1

/* Select sd card interface
0: SD_MMC
1: SD interface over SPI
*/
#define SD_CARD_SYSTEM  0

/* Enable control power sd card 
0: not use
1: use
*/
#define SD_POWER_ENABLE 1

/* Nand memory file system
0: SPIFFS
1: LITTLEFS
2: FATFS

Usage ESP32 Sketch Data Upload
1. Download esp32fs.zip
  - link: https://github.com/lorol/arduino-esp32fs-plugin/releases
  - Unpack the tool into "C:\Users\HUY-GAMING\Documents\Arduino\tools\ESP32FS\tool\esp32fs.jar"

2. Download x86_64-w64-mingw32-mklittlefs-295fe9b.zip
  - Link: https://github.com/earlephilhower/mklittlefs/releases
  - Unpack the file into C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mklittlefs\mklittlefs.exe

3. Download mkfatfs_v1.0.exe
  - Link: https://github.com/labplus-cn/mkfatfs/releases/tag/v1.0
  - Unpack the file into C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mkfatfs\mkfatfs.exe

4. mkspiffs.exe is already in directory
  - C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mkspiffs\mkspiffs.exe

5. Install LITTLEFS
  - Link: https://github.com/lorol/LITTLEFS
  - Or Manage Libraries with search "LITTLEFS"

6. Upload FS
  - Restart Arduino IDE after any steps above
  - Tools --> ESP32 Sketch Data Upload --> [LITTLEFS, SPIFFS, FATFS, Erase Flash]
*/
#define USE_NAND_FS_SYSTEM 1

/* Include header fife system */
#if (defined USE_NAND_FS_SYSTEM) && (USE_NAND_FS_SYSTEM == 2)
  #define NAND_FS_SYSTEM FFat
  #include <FS.h>
  #include <FFat.h>
#elif (defined USE_NAND_FS_SYSTEM) && (USE_NAND_FS_SYSTEM == 1)  
  #define NAND_FS_SYSTEM LITTLEFS
  #include <FS.h>
  #include <LITTLEFS.h> 
#else
  #define NAND_FS_SYSTEM SPIFFS
  #include <SPIFFS.h>
#endif

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#include <FS.h>
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1)
#include <SD.h>
#include <SPI.h>
#else
#include <SD_MMC.h>
#endif
#endif

#endif
