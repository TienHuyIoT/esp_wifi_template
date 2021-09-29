#include <Arduino.h>
#include <functional> // std::function
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPBlinkGPIO.h"
#include "THIoT_SDFSClass.h"
#include "THIoT_SerialTrace.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPWebserver.h"
#include "THIoT_WebserverURLHandle.h"
#include "THIoT_ESPEEPromParams.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_ESPResetReason.h"
#include "THIoT_ESPAsyncEasyNTP.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_FactoryButton.h"
#include "esp_led_status.h"

/* Private macro -------------------------------------------------------------*/
#define MAIN_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#define MAIN_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[MAIN]", __VA_ARGS__)
#define MAIN_FUNCTION_LOG(...) FS_FUNCTION_TAG_LOGI("[MAIN]", __VA_ARGS__)

/* Private variables ---------------------------------------------------------*/
ESPWebserver webServer;
FactoryButton factorySysParams(FACTORY_INPUT_PIN);
ESPBlinkGPIO LEDStatus(LED_STATUS_GPIO, HIGH);

void setup()
{
#ifdef ESP32
    SERIAL_PORT.begin(SERIAL_BAUDRATE, SERIAL_8N1, -1, 1);
#elif defined(ESP8266)
    SERIAL_PORT.begin(SERIAL_BAUDRATE, SERIAL_8N1);
#endif
    const char *build_time = __DATE__ " " __TIME__ " GMT";
    MAIN_TAG_CONSOLE("\r\n\r\nbuild_time: %s", build_time);
    MAIN_TAG_CONSOLE("==== Firmware version %u.%u.%u ====\r\n",
                     FW_VERSION_MAJOR,
                     FW_VERSION_MINOR,
                     FW_VERSION_BUILD);
    
    MAIN_TAG_CONSOLE("Reset reason %s", esp_reset_reason_str().c_str());

    // Enable watch dog timer         
    // WDT_TIMEOUT_VALUE only effected for ESP32
    // ESP8266 refer https://techtutorialsx.com/2017/01/21/esp8266-watchdog-functions/
    wdt_enable(WDT_TIMEOUT_VALUE);

    
    // Make updating led status by wifi status
    LEDStatus.setCycleCallbacks(new ESPLedCycleBlinkCallbacks());
    ESPWifi.onLedStatus(std::bind(&ESPBlinkGPIO::statusUpdate, &LEDStatus, std::placeholders::_1));    
    LEDStatus.statusUpdate(ESPLedCycleBlinkCallbacks::BLINK_NORMAL);

    // Load params form eeprom memory
    EEPParams.load();


    // Initialize and auto format nand memory file system
#ifdef ESP32
    NAND_FS_SYSTEM.begin(true);
#elif defined(ESP8266)
    NAND_FS_SYSTEM.begin();
#endif
    FSHandle.listDir(NAND_FS_SYSTEM, "/", 0);


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
    FSHandle.listDir(SD_FS_SYSTEM, "/", 0);
#endif // (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)


    // Always initialize after NAND_FS_SYSTEM.begin();
    // Because some function of system will need some params 
    // load from a file system to initial.
    ESPConfig.load(&NAND_FS_SYSTEM);

    // Must be load after ESPConfig.load()
    // Because it needs some parameters to configure time zone.
    ESPTime.load();

    // Save reset reason into log file.
    MAIN_FUNCTION_LOG("reset reason %s", esp_reset_reason_str().c_str());

    // Check starting condition for ethernet.
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
    // If the ethernet is enabled, the Wifi will not be allowed to initialize .
    Ethernet.begin();
    ESPWifi.begin(!Ethernet.isEnable());
#else
    // Init wifi and accompanied services 
    ESPWifi.begin();
#endif
    
    // register callback handle http request
    webServer.setHandleCallbacks(new WebserverURLHandle());
    webServer.begin();

    // handle factory system params by hold button over 2s
    // factorySysParams.onFactory(std::bind(&ESPSysParams::setDefault, &ESPConfig));
    factorySysParams.onFactory([](){
        ESPConfig.setDefault();
        LEDStatus.statusUpdate(ESPLedCycleBlinkCallbacks::BLINK_FACTORY_SYSTEM_PARAMS);
    });
    factorySysParams.begin();
}

void loop()
{
    wdt_reset();
    ESPWifi.loop();
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    Ethernet.loop();
#endif
}
