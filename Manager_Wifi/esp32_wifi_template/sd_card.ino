#include "app_config.h"
#include "sd_card.h"
#include "log_report.h"
#include "fs_handle.h"
#include "console_dbg.h"

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
#define SD_FS_PRINTF(f_, ...) SD_FS_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)
#define SD_FS_PRINTFLN(f_, ...) SD_FS_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)

/* Private variables ---------------------------------------------------------*/
static bool sd_card_status = false;

extern void log_report(uint8_t log_id, char *p_log);

bool sd_card_status_isOK(void)
{
  return (true == sd_card_status);
}

void sd_card_init(void)
{
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
    SD_FS_PRINTFLN("\r\nCard Mount Failed");
    log_report(LOG_REPORT_SD, (char *)"Card Mount Failed");
    sd_card_status = false;
    return;
  }
#else
  SD_NSS_PINMODE_INIT();
  SD_NSS_RELEASE();
  // Init SPI driver.
  // We must be init SPI PIN the first than Init SD card
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_NSS_PIN);
  // Init SD card with SPI clock = 80Mhz
  // https://github.com/espressif/esp-idf/issues/1008
  if (!SD_FS_SYSTEM.begin(SD_NSS_PIN, SPI, 80E6))
  {
    SD_FS_PRINTFLN("\r\nCard Mount Failed");
    log_report(LOG_REPORT_SD, (char *)"Card Mount Failed");
    sd_card_status = false;
    return;
  }
#endif
  sd_card_status = true;
  uint8_t cardType = SD_FS_SYSTEM.cardType();

  if (cardType == CARD_NONE)
  {
    SD_FS_PRINTFLN("\r\nNo SD_FS_SYSTEM card attached");
    return;
  }

  SD_FS_PRINTF("\r\nSD_FS_SYSTEM Card Type: ");
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

  uint64_t cardSize = SD_FS_SYSTEM.cardSize() / (1024 * 1024);
  SD_FS_PRINTF("SD_FS_SYSTEM Card Size: %lluMB\r\n", cardSize);

  log_report(LOG_REPORT_SD, (char *)"SD Init OK");

#if (0)
  createDir(SD_FS_SYSTEM, "/mydir");
  listDir(SD_FS_SYSTEM, "/", 0);
  removeDir(SD_FS_SYSTEM, "/mydir");
  listDir(SD_FS_SYSTEM, "/", 2);
  writeFile(SD_FS_SYSTEM, "/hello.txt", "Hello ");
  appendFile(SD_FS_SYSTEM, "/hello.txt", "World!\n");
  readFile(SD_FS_SYSTEM, "/hello.txt");
  deleteFile(SD_FS_SYSTEM, "/foo.txt");
  renameFile(SD_FS_SYSTEM, "/hello.txt", "/foo.txt");
  readFile(SD_FS_SYSTEM, "/foo.txt");
  testFileIO(SD_FS_SYSTEM, "/test.txt");
  SD_FS_PRINTF("Total space: %lluMB\n", SD_FS_SYSTEM.totalBytes() / (1024 * 1024));
  SD_FS_PRINTF("Used space: %lluMB\n", SD_FS_SYSTEM.usedBytes() / (1024 * 1024));
#endif
}

#endif // (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
