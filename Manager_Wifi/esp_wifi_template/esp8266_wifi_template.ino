#include <Arduino.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_SDFSClass.h"
#include "THIoT_SerialTrace.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPWebserver.h"
#include "THIoT_WebserverURLHandle.h"
#include "THIoT_ESPEEPromParams.h"
#include "THIoT_ESPEthernet.h"

/* Private macro -------------------------------------------------------------*/
#define MAIN_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[MAIN]", __VA_ARGS__)
#define MAIN_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)

/* Private variables ---------------------------------------------------------*/
const char *build_time = __DATE__ " " __TIME__ " GMT";
ESPWebserver webServer;

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

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
#if (defined ETH_GPIO_ENABLE) && (ETH_GPIO_ENABLE != -1)
  ETH_GPIO_ENABLE_INIT();
  if(ETH_STATUS_IS_ON())
  {
    Ethernet.enable();
  }
  else
  {
    Ethernet.disable();
  }
#else
    Ethernet.enable();
#endif
#endif

    // Load params form eeprom memory
    EEPParams.load();

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#if (defined SD_SPI_INTERFACE) && (SD_SPI_INTERFACE == 1)
    /* Init SPI first */
#ifdef ESP32
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
    /* Init Sd Card second*/
    HTH_sdCard.begin(SPI);
#elif defined(ESP8266)
    SPI.begin();
    /* Init Sd Card second*/
    HTH_sdCard.begin();
#endif // ESP32
#else
    // SDMMC interface only with ESP32
    HTH_sdCard.begin();
#endif // (defined SD_SPI_INTERFACE) && (SD_SPI_INTERFACE == 1)
#endif // (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)

    // Initialize and auto format nand memory file system
#ifdef ESP32
    NAND_FS_SYSTEM.begin(true);
#elif defined(ESP8266)
    NAND_FS_SYSTEM.begin();
#endif

    /* List file in nand memory file system */
    FSHandle.listDir(NAND_FS_SYSTEM, "/", 0);
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
    FSHandle.listDir(SD_FS_SYSTEM, "/", 0);
#endif

    // Always initialize after NAND_FS_SYSTEM.begin();
    // Because some function of system will need params 
    // load from file system for initial.
    ESPConfig.load(&NAND_FS_SYSTEM);
    
    // Init system time with params option load from the file system
    ESPTime.load();

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    // If the ethernet is enable, the wifi will not init.
    Ethernet.begin();
    ESPWifi.begin(!Ethernet.isEnable());
#else
    // Init wifi and accompanied services 
    ESPWifi.begin();
#endif

    // register callback handle http request
    webServer.setHandleCallbacks(new WebserverURLHandle());
    webServer.begin();
}

void loop()
{
    ESPWifi.loop();
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    Ethernet.loop();
#endif
}
