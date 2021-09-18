#include "app_config.h"
#include "hth_fs_handle.h"
#include "hth_console_dbg.h"
#include "hth_esp_sdcard.h"

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
/* Includes ------------------------------------------------------------------*/
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1)
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
hth_sdfs::hth_sdfs()
: fs::FS(FSImplPtr(new sdfs::SDFSImpl()))
{
}

hth_sdfs::~hth_sdfs(){}

uint8_t hth_sdfs::type() {
  if (!_impl) {
      SD_FS_PRINTFLN("[type] FS not mounted\n");
      return CARD_NONE;
  }
    sdfs::SDFSImpl* sd = static_cast<sdfs::SDFSImpl*>(getImpl().get());
    return sd->type();
}

bool hth_sdfs::info64(FSInfo64& info)
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

hth_sdfs HTH_sdfs;
#endif
#endif

hth_esp_sdcard::hth_esp_sdcard(/* args */)
{
  _sdCardStatus = false;
}

hth_esp_sdcard::~hth_esp_sdcard()
{
}

#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1) && (defined ESP32)
void hth_esp_sdcard::begin(SPIClass &spi)
#else
void hth_esp_sdcard::begin()
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
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 0)
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
  SD_FS_SYSTEM.setConfig(SDFSConfig(SD_NSS_PIN, SPI_FULL_SPEED));
  if (!SD_FS_SYSTEM.begin())
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

  SD_FS_PRINTF("SD_FS_SYSTEM Card Type: ");
  if (cardType == CARD_MMC)
  {
    SD_FS_PRINTFLN("MMC");
  }
  else if (cardType == CARD_SD)
  {
    SD_FS_PRINTFLN("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    SD_FS_PRINTFLN("SDHC");
  }
  else
  {
    SD_FS_PRINTFLN("UNKNOWN");
  }

  SD_FS_PRINTF("SD_FS_SYSTEM Card Size: %lluMB\r\n", cardSize);
  SD_FS_PRINTF("SD_FS_SYSTEM Card Space: %lluMB\r\n", usedBytes);

#if (0)
  HTH_fsHandle.createDir(SD_FS_SYSTEM, "/mydir");
  HTH_fsHandle.listDir(SD_FS_SYSTEM, "/", 0);
  HTH_fsHandle.removeDir(SD_FS_SYSTEM, "/mydir");
  HTH_fsHandle.listDir(SD_FS_SYSTEM, "/", 2);
  HTH_fsHandle.writeFile(SD_FS_SYSTEM, "/hello.txt", "Hello ");
  HTH_fsHandle.appendFile(SD_FS_SYSTEM, "/hello.txt", "World!\n");
  HTH_fsHandle.readFile(SD_FS_SYSTEM, "/hello.txt");
  HTH_fsHandle.deleteFile(SD_FS_SYSTEM, "/foo.txt");
  HTH_fsHandle.renameFile(SD_FS_SYSTEM, "/hello.txt", "/foo.txt");
  HTH_fsHandle.readFile(SD_FS_SYSTEM, "/foo.txt");
  HTH_fsHandle.testFileIO(SD_FS_SYSTEM, "/test.txt");
#endif
}

hth_esp_sdcard HTH_sdCard;

#endif // (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
