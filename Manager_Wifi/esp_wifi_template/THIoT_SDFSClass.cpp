#include "THIoT_ESPConfig.h"
#include "THIoT_ESPFSHandle.h"
#include "THIoT_SerialTrace.h"
#include "THIoT_SDFSClass.h"

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
#define SD_FS_PORT CONSOLE_PORT
#define SD_FS_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define SD_FS_PRINTFLN(...) CONSOLE_TAG_LOGI("[SD]", __VA_ARGS__)

#ifdef ESP8266
#if(HTH_SFDS_HANDLE)
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
      SD_FS_PRINTFLN("[type] FS not mounted\n");
      return CARD_NONE;
  }
    sdfs::SDFSImpl* sd = static_cast<sdfs::SDFSImpl*>(getImpl().get());
    return sd->type();
}

bool SDFSClass::info64(FSInfo64& info)
{
  if (!_impl) {
      SD_FS_PRINTFLN("[info64] FS not mounted\n");
      return false;
  }
  SD_FS_PRINTFLN("[info64] updated\n");
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
void ESPSdCard::begin(SPIClass &spi)
#else
void ESPSdCard::begin()
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
  if (!SD_FS_SYSTEM.begin())
  {
    SD_FS_PRINTFLN("Card Mount Failed");
    _sdCardStatus = false;
    return;
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
    SD_FS_PRINTFLN("Card Mount Failed");
    _sdCardStatus = false;
    return;
  }
#endif
  SD_FS_PRINTFLN("Card Mount Succeed");
  _sdCardStatus = true;

#ifdef ESP32
  cardSize = SD_FS_SYSTEM.cardSize() / (1024ULL * 1024ULL);
  usedBytes = SD_FS_SYSTEM.usedBytes() / (1024ULL * 1024ULL);
  cardType = SD_FS_SYSTEM.cardType();  
#elif defined(ESP8266)
#if(HTH_SFDS_HANDLE)
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

  SD_FS_PRINTFLN("cardType = %u", cardType);
  if (cardType == CARD_NONE)
  {
    SD_FS_PRINTFLN("No SD_FS_SYSTEM card attached");
    return;
  }

  if (cardType == CARD_MMC)
  {
    SD_FS_PRINTFLN("Type: MMC");
  }
  else if (cardType == CARD_SD)
  {
    SD_FS_PRINTFLN("Type: SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    SD_FS_PRINTFLN("Type: SDHC");
  }
  else
  {
    SD_FS_PRINTF("Type: UNKNOWN");
  }

  SD_FS_PRINTFLN("Card Size: %lluMB\r\n", cardSize);
  SD_FS_PRINTFLN("Card Space: %lluMB\r\n", usedBytes);

#if (0)
  FSHandle.createDir(SD_FS_SYSTEM, "/mydir");
  FSHandle.listDir(SD_FS_SYSTEM, "/", 0);
  FSHandle.removeDir(SD_FS_SYSTEM, "/mydir");
  FSHandle.listDir(SD_FS_SYSTEM, "/", 2);
  FSHandle.writeFile(SD_FS_SYSTEM, "/hello.txt", "Hello ");
  FSHandle.appendFile(SD_FS_SYSTEM, "/hello.txt", "World!\n");
  FSHandle.readFile(SD_FS_SYSTEM, "/hello.txt");
  FSHandle.deleteFile(SD_FS_SYSTEM, "/foo.txt");
  FSHandle.renameFile(SD_FS_SYSTEM, "/hello.txt", "/foo.txt");
  FSHandle.readFile(SD_FS_SYSTEM, "/foo.txt");
  FSHandle.testFileIO(SD_FS_SYSTEM, "/test.txt");
#endif
}

ESPSdCard HTH_sdCard;

#endif // (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
