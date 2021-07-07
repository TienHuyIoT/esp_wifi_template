/* Includes ------------------------------------------------------------------*/

#include "app_config.h"
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1)
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define SPIFFS_PORT Serial
#define SPIFFS_PRINTF(f_, ...) SPIFFS_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    SPIFFS_PORT.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        SPIFFS_PORT.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        SPIFFS_PORT.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            SPIFFS_PORT.print("  DIR : ");
            SPIFFS_PORT.print(file.name());
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            SPIFFS_PORT.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\r\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            SPIFFS_PORT.print("  FILE: ");
            SPIFFS_PORT.print(file.name());
            SPIFFS_PORT.print("  SIZE: ");
            SPIFFS_PORT.print(file.size());
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            SPIFFS_PORT.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\r\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}
