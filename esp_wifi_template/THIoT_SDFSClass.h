#ifndef	__SDFS_CLASS_H
#define __SDFS_CLASS_H

#include <Arduino.h>
#include "THIoT_ESPConfig.h"

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#if (defined SD_SPI_INTERFACE) && (SD_SPI_INTERFACE == 1) && (defined ESP32)
#include <SPI.h>
#endif
#include "THIoT_ESPBoard.h"

#if (SD_SPI_INTERFACE == 0)
#define SD_FS_SYSTEM SD_MMC
#else
#ifdef ESP32
#define SD_FS_SYSTEM SD
#elif defined(ESP8266)
#include <SDFS.h>

#define HTH_SFDS_HANDLE 1

typedef enum {
    CARD_NONE,
    CARD_MMC,
    CARD_SD,
    CARD_SDHC,
    CARD_UNKNOWN
} sdcard_type_t;

/* Make the same API with SD class */
#if (HTH_SFDS_HANDLE)
class SDFSClass : public fs::FS
{

public:
    SDFSClass();
    ~SDFSClass();

    boolean begin(uint8_t csPin, uint32_t cfg = SPI_HALF_SPEED);
    uint8_t type();

    /**
     * SDFS::info64 geting is very slow by freeClusterCount()
     * So we declared owner info64 function to improvement speed
    */
    bool info64(FSInfo64& info);
private:
};

extern SDFSClass SDFSHandle;
#define SD_FS_SYSTEM SDFSHandle
#else
// using SDFS instead of SD for almost every case
#define SD_FS_SYSTEM SDFS
#endif // #if (0)

#endif // defined(ESP8266)
#endif // (SD_SPI_INTERFACE == 0)

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

class ESPSdCard
{
private:
    bool _sdCardStatus;
public:
    ESPSdCard(/* args */);
    ~ESPSdCard();
    bool statusIsOk() { return _sdCardStatus; }
#if (defined SD_SPI_INTERFACE) && (SD_SPI_INTERFACE == 1) && (defined ESP32)
    void begin(SPIClass &spi);
#else
    void begin();
#endif
};

extern ESPSdCard HTH_sdCard;

#endif // (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)

#endif // __SDFS_CLASS_H