#include <Arduino.h>
#include "app_config.h"
#include "hth_console_dbg.h"
#include "hth_esp_sys_rtc.h"
#include "hth_esp_wifi.h"
#include "hth_esp_sys_data.h"
#include "async_webserver.h"

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
    MAIN_TAG_CONSOLE("build_time: %s", build_time);
    MAIN_TAG_CONSOLE("==== Firmware version %u.%u.%u ====\r\n",
                     FW_VERSION_MAJOR,
                     FW_VERSION_MINOR,
                     FW_VERSION_BUILD);

#ifdef ESP32
    /* Init nand memory file system */
    NAND_FS_SYSTEM.begin(true);
#elif defined(ESP8266)
    NAND_FS_SYSTEM.begin();
#endif
    // Always initialize after NAND_FS_SYSTEM.begin();
    // Because some function of system will need params 
    // load from file system for initial.
    WFDataFile.begin();
    
    HTH_sysTime.begin();

    HTH_espWifi.begin();

    HTH_asyncServer.begin();
}

void loop()
{
    HTH_espWifi.loop();
}
