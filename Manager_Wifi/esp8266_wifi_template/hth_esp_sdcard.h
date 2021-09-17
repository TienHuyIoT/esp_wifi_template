#ifndef	_SD_CARD_
#define _SD_CARD_

#include <Arduino.h>
#include "app_config.h"

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1) && (defined ESP32)
#include <SPI.h>
#endif
#include "board.h"

#if (SD_CARD_SYSTEM == 0)
#define SD_FS_SYSTEM SD_MMC
#else
#ifdef ESP32
#define SD_FS_SYSTEM SD
#elif defined(ESP8266)
// using SDFS instead of SD for almost every case
#define SD_FS_SYSTEM SDFS
#endif
#endif

/*
SD POWER
DIR: 	OUTPUT
HIGH:	SD off
LOW:	SD on
*/
#if (defined SD_POWER_ENABLE) && (SD_POWER_ENABLE == 1)
#if (SD_POWER_PIN != -1)
#define SD_POWER_PINMODE_INIT()         pinMode(SD_POWER_PIN, OUTPUT_OPEN_DRAIN)
#define SD_POWER_ON()                   digitalWrite(SD_POWER_PIN, LOW)
#define SD_POWER_OFF()                  digitalWrite(SD_POWER_PIN, HIGH)
#else
#define SD_POWER_PINMODE_INIT() 
#define SD_POWER_ON()           
#define SD_POWER_OFF()          
#endif
#endif

/*
SD INTERFACE
*/
#define SD_NSS_PINMODE_INIT()           pinMode(SD_NSS_PIN, OUTPUT)
#define SD_NSS_RELEASE()                digitalWrite(SD_NSS_PIN, HIGH)
#define SD_SCK_PIN     			        SPI_SCK_PIN
#define SD_MISO_PIN     		        SPI_MISO_PIN
#define SD_MOSI_PIN     		        SPI_MOSI_PIN   

class hth_esp_sdcard
{
private:
    bool _sdCardStatus;
public:
    hth_esp_sdcard(/* args */);
    ~hth_esp_sdcard();
    bool statusIsOk() { return _sdCardStatus; }
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1) && (defined ESP32)
    void begin(SPIClass &spi);
#else
    void begin();
#endif
};

extern hth_esp_sdcard HTH_sdCard;

#endif // (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)

#endif // _SD_CARD_