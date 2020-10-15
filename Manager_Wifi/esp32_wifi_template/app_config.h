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

/* Enable sd card */
#define SD_CARD_ENABLE  0
/* Select sd card interface
0: SD_MMC
1: SD interface over SPI
*/
#define SD_CARD_SYSTEM  1

#if (SD_CARD_SYSTEM == 0)
#define SD_FS_SYSTEM SD_MMC
#else
#define SD_FS_SYSTEM SD
#define SD_CARD_NSS_PIN 16
#endif

/* Nand memory file system */
#define NAND_FS_SYSTEM SPIFFS

#endif
