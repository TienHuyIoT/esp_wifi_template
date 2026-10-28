#include <driver/sdmmc_types.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPFSHandle.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_ESPSDFSClass.h"

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
/* Includes ------------------------------------------------------------------*/
#if (defined SD_SPI_INTERFACE) && (SD_SPI_INTERFACE == 1)
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define SD_FS_TAG_CONSOLE(...)  //CONSOLE_TAG_LOGI("[SD]", __VA_ARGS__)

#define SD_FS_TAG_ESP_LOG(...)  ESP_FS_TAG_LOGI("[SD]", __VA_ARGS__)

RTC_NOINIT_ATTR int noInitSdFormat;

#ifdef ESP8266
#if(HTH_SDFS_HANDLE)
SDFSClass::SDFSClass()
: fs::FS(FSImplPtr(new sdfs::SDFSImpl()))
{
}

SDFSClass::~SDFSClass(){}

boolean SDFSClass::begin(uint8_t csPin, uint32_t cfg) 
{
  sdfs::SDFSImpl* sd = static_cast<sdfs::SDFSImpl*>(getImpl().get());
  sd->setConfig(SDFSConfig(csPin, cfg));
  return (boolean)sd->begin();
}

uint8_t SDFSClass::type() {
  if (!_impl) {
      SD_FS_TAG_CONSOLE("[type] FS not mounted\n");
      return CARD_NONE;
  }
    sdfs::SDFSImpl* sd = static_cast<sdfs::SDFSImpl*>(getImpl().get());
    return sd->type();
}

bool SDFSClass::info64(FSInfo64& info)
{
  if (!_impl) {
      SD_FS_TAG_CONSOLE("[info64] FS not mounted\n");
      return false;
  }
  SD_FS_TAG_CONSOLE("[info64] updated\n");
  sdfs::SDFSImpl* sd = static_cast<sdfs::SDFSImpl*>(getImpl().get());
  info.blockSize = 512;
  info.usedBytes = 0x40000000ULL; // add for fun
  info.totalBytes = (uint64_t)sd->totalClusters() * (uint64_t)sd->blocksPerCluster() * (uint64_t)info.blockSize;
  return true;
}

SDFSClass SDFSHandle;
#endif
#endif

ESPSdCard::ESPSdCard(/* args */)
{
  _sdCardStatus = false;
}

ESPSdCard::~ESPSdCard()
{
}

#if (defined SD_SPI_INTERFACE) && (SD_SPI_INTERFACE == 1) && (defined ESP32)
boolean ESPSdCard::begin(SPIClass &spi)
#else
boolean ESPSdCard::begin()
#endif
{
  uint8_t cardType;
  uint64_t cardSize;
  uint64_t usedBytes;

#if (defined SD_POWER_ENABLE) && (SD_POWER_ENABLE == 1)
  SD_POWER_PINMODE_INIT();
  SD_POWER_OFF();
  delay(10); // add timeout to supply power to sd card
  SD_POWER_ON();
  delay(10); // add timeout to supply power to sd card
#endif
#if (defined SD_SPI_INTERFACE) && (SD_SPI_INTERFACE == 0)
  if (!SD_FS_SYSTEM.begin("/sdcard", false, isFormatON(), SDMMC_FREQ_HIGHSPEED, SDMMC_MAX_OPEN_FILES))
  {
    SD_FS_TAG_CONSOLE("Card Mount Failed");
    _sdCardStatus = false;
    return false;
  }
#else
  SD_NSS_PINMODE_INIT();
  SD_NSS_RELEASE();
#ifdef ESP32
  // Init SD card with SPI clock = 80Mhz
  // https://github.com/espressif/esp-idf/issues/1008
  if (!SD_FS_SYSTEM.begin(SD_NSS_PIN, spi, 80E6))
#elif defined(ESP8266)
  if (!SD_FS_SYSTEM.begin(SD_NSS_PIN, SPI_FULL_SPEED))
#endif
  {
    SD_FS_TAG_CONSOLE("Card Mount Failed");
    _sdCardStatus = false;
    return false;
  }
#endif
  SD_FS_TAG_CONSOLE("Card Mount Succeed");
  _sdCardStatus = true;

#ifdef ESP32
  cardSize = SD_FS_SYSTEM.cardSize() / (1024ULL * 1024ULL);
  usedBytes = SD_FS_SYSTEM.usedBytes() / (1024ULL * 1024ULL);
  cardType = SD_FS_SYSTEM.cardType();  
#elif defined(ESP8266)
#if(HTH_SDFS_HANDLE)
  FSInfo64 fs_info;
  SD_FS_SYSTEM.info64(fs_info);
  cardSize = fs_info.totalBytes / (1024ULL * 1024ULL);
  usedBytes = fs_info.usedBytes / (1024ULL * 1024ULL);
  cardType = SD_FS_SYSTEM.type();
#else
  cardSize = SD.size64() / (1024ULL * 1024ULL);
  usedBytes = 0x40000000ULL / (1024ULL * 1024ULL); // add for fun
  cardType = SD.type();
#endif
#endif

  SD_FS_TAG_CONSOLE("cardType = %u", cardType);
  if (cardType == CARD_NONE)
  {
    SD_FS_TAG_CONSOLE("No SD_FS_SYSTEM card attached");
    return false;
  }

  if (cardType == CARD_MMC)
  {
    SD_FS_TAG_CONSOLE("Type: MMC");
  }
  else if (cardType == CARD_SD)
  {
    SD_FS_TAG_CONSOLE("Type: SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    SD_FS_TAG_CONSOLE("Type: SDHC");
  }
  else
  {
    SD_FS_TAG_CONSOLE("Type: UNKNOWN");
  }

  SD_FS_TAG_CONSOLE("Card Size %lluMB, used %lluMB", cardSize, usedBytes);

  if (isFormatON()) {
    formatOFF();
    if (SD_FS_SYSTEM.format() == ESP_OK) {
      SD_FS_TAG_ESP_LOG("Format SD card succeed!");
    }
    else {
      SD_FS_TAG_ESP_LOG("Formatting SD card failure!");
    }

    CONSOLE_FLUSH();
    delay(150);
    ESP.restart();
  }
  return true;
}

/**
 * @brief get the free space of SD card
*/
uint64_t ESPSdCard::freeSpace() {
  uint64_t space = FS_HANDLE_SD_FREE_SPACE_SIZE_LIMIT;
  if (_sdCardStatus) {
    uint64_t cardSize = SD_FS_SYSTEM.cardSize();
    uint64_t usedBytes = SD_FS_SYSTEM.usedBytes();
    if (cardSize > usedBytes) {
      space = cardSize - usedBytes;
    }
  }
  SD_FS_TAG_CONSOLE("SD free space: %llu", space);
  return space;
}

ESPSdCard SDCard;

#endif // (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
