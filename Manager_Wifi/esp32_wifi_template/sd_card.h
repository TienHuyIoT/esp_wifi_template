#ifndef	_SD_CARD_
#define _SD_CARD_

#include "app_config.h"

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#include "board.h"

#if (SD_CARD_SYSTEM == 0)
#define SD_FS_SYSTEM SD_MMC
#else
#define SD_FS_SYSTEM SD
#endif

/*
SD POWER
DIR: 	OUTPUT
HIGH:	SD off
LOW:	SD on
*/
#if (defined SD_POWER_ENABLE) && (SD_POWER_ENABLE == 1)
#define SD_POWER_PIN     		        22
#define SD_POWER_PINMODE_INIT()         pinMode(SD_POWER_PIN, OUTPUT_OPEN_DRAIN)
#define SD_POWER_ON()                   digitalWrite(SD_POWER_PIN, LOW)
#define SD_POWER_OFF()                  digitalWrite(SD_POWER_PIN, HIGH)
#endif

/*
SD INTERFACE
*/
#define SD_NSS_PIN				        16 
#define SD_NSS_PINMODE_INIT()           pinMode(SD_NSS_PIN, OUTPUT)
#define SD_NSS_RELEASE()                digitalWrite(SD_NSS_PIN, HIGH)
#define SD_SCK_PIN     			        SPI_SCK_PIN
#define SD_MISO_PIN     		        SPI_MISO_PIN
#define SD_MOSI_PIN     		        SPI_MOSI_PIN   

#endif

#endif