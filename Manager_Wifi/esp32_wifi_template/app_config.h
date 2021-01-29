#ifndef	_APP_CONFIG_
#define _APP_CONFIG_

// <h> Hardware Version

//==========================================================
// <o> HW_VERSION_STRING

#ifndef HW_VERSION_STRING
#define HW_VERSION_STRING "RFID.REVA.V2.0"
#endif

// </h>

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


#ifndef WDT_TIMEOUT_VALUE
#define WDT_TIMEOUT_VALUE  2000 /* ms */
#endif

/* OTA Arduino
0: Disable
1: Enable
*/
#define OTA_ARDUINO_ENABLE  1

/* DNS Server
0: Disable
1: Enable
*/
#define DNS_SERVER_ENABLE  1

/* LAN network
0: Disable
1: Enable
*/
#define ETH_ENABLE    1

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
#define SD_POWER_ENABLE 0

/* Nand memory file system
0: SPIFFS
1: LITTLEFS
2: FATFS
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

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
#include "lan8720a_cfg.h"
#endif

#endif
