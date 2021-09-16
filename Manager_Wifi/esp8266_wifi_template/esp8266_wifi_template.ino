#include <Arduino.h>
#include "app_config.h"
#include "hth_esp_sdcard.h"
#include "hth_console_dbg.h"
#include "hth_esp_sys_rtc.h"
#include "hth_esp_wifi.h"
#include "hth_esp_sys_data.h"
#include "hth_webserver.h"
#include "hth_httpserver_url.h"
#include "hth_esp_eeprom.h"

/* Private macro -------------------------------------------------------------*/
#define MAIN_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[MAIN]", __VA_ARGS__)
#define MAIN_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)

/* Private variables ---------------------------------------------------------*/
const char *build_time = __DATE__ " " __TIME__ " GMT";
async_webserver HTH_asyncServer;

void setup()
{
#ifdef ESP32
    CONSOLE_PORT.begin(CONSOLE_BAUDRATE, SERIAL_8N1, -1, 1);
#elif defined(ESP8266)
    CONSOLE_PORT.begin(CONSOLE_BAUDRATE, SERIAL_8N1);
#endif
    MAIN_TAG_CONSOLE("\r\n\r\nbuild_time: %s", build_time);
    MAIN_TAG_CONSOLE("==== Firmware version %u.%u.%u ====\r\n",
                     FW_VERSION_MAJOR,
                     FW_VERSION_MINOR,
                     FW_VERSION_BUILD);

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1)
    /* Init SPI first */
#ifdef ESP32
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
    /* Init Sd Card second*/
    HTH_sdCard.begin(SPI);
#elif defined(ESP8266)
    SPI.begin();
    /* Init Sd Card second*/
    HTH_sdCard.begin();
#endif
#endif
#endif

    HTH_espEEPROM.begin();

#ifdef ESP32
    /* Init nand memory file system */
    NAND_FS_SYSTEM.begin(true);
#elif defined(ESP8266)
    NAND_FS_SYSTEM.begin();
#endif
    /* List file in nand memory file system */
    HTH_fsHandle.listDir(NAND_FS_SYSTEM, "/", 0);

    // Always initialize after NAND_FS_SYSTEM.begin();
    // Because some function of system will need params 
    // load from file system for initial.
    WFDataFile.begin();
    
    HTH_sysTime.begin();

    HTH_espWifi.begin();

    HTH_asyncServer.setHandleCallbacks(new requestHandler());
    HTH_asyncServer.begin();
}

void loop()
{
    HTH_espWifi.loop();
}
