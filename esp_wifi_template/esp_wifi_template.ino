#include <Arduino.h>
#include <functional> // std::function
#include <sdkconfig.h>
#include <esp_idf_version.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <NetBIOS.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <driver/rtc_io.h>
#if (ESP_IDF_VERSION_MAJOR >= 4) // IDF 4+
#include <aes/esp_aes.h>
#include <esp_sntp.h>
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include <esp32/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32S2
#include <esp32s2/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32C3
#include <esp32c3/rom/rtc.h>
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include <hwcrypto/aes.h>
#include <rom/rtc.h>
#include <lwip/apps/sntp.h>
#endif

#include <soc/rtc.h>
#include <time.h>
#include <Ticker.h>
#include "THIoT_APPConfig.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_PFcUtil.h"
#include "THIoT_ESPTimeType.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPBoard.h"
#include "THIoT_ESPFSEditor.h"
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_ESPWebserverURLHandle.h"
#include "THIoT_ESPWebserver.h"
#include "THIoT_ESPWebsocket.h"
#include "THIoT_ESPWsDataHandler.h"
#include "THIoT_PFAppFifoExtra.h"
#include "THIoT_PFFSService.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_PFFrame.h"
#include "THIoT_PFFrameCom.h"
#include "THIoT_PFTicker.h"
#include "THIoT_PFcList.h"
#include "THIoT_ESPSntp.h"
#include "THIoT_ESPFSHandle.h"
#include "THIoT_ESPSDFSClass.h"
#include "THIoT_PFStateMachine.h"
#include "THIoT_PFFactoryButton.h"
#include "THIoT_ESPBlinkGPIO.h"
#include "THIoT_PFLedStatus.h"
#include "THIoT_ESPResetReason.h"
#include "THIoT_ESPTimerWakeUp.h"
#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_ESPEEPromParams.h"
#include "THIoT_APPButtonService.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPCalendarSchedule.h"
#include "THIoT_ESPPingService.h"

/* Private macro -------------------------------------------------------------*/
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#define MAIN_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[MAIN]", __VA_ARGS__)
#define MAIN_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#define RS485_LOG_DBG_PRINT(...) SERIAL_TAG_LOGI("[RS485_LOG]", __VA_ARGS__)

#define MAIN_TAG_LOG(...) FS_TAG_LOGI("[MAIN]", __VA_ARGS__)
#define MAIN_FUNCTION_LOG(...) FS_FUNCTION_TAG_LOGI("[MAIN]", __VA_ARGS__)
#define MAIN_LOG(...) FS_PRINTF(__VA_ARGS__)

/* Private variables ---------------------------------------------------------*/
const char *build_time = "Thu " __DATE__ " " __TIME__ " GMT";
#define PING_GATEWAY_ID         PING_HOST1_INDEX
#define PING_INTERNET_ID        PING_HOST2_INDEX
ESPPingService pingGateway;
ESPPingService pingInternet;
ESPWebServer webServer;
#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
FactoryButton factorySysParams(FACTORY_INPUT_PIN);
#endif
#if (defined LED_STATUS_GPIO) && (LED_STATUS_GPIO != -1)
ESPBlinkGPIO LEDStatus(LED_STATUS_GPIO, HIGH);
#endif

void setup() {
    using namespace std::placeholders;
#if (MUTEX_PLATFORM_TICKER == 1)
    ticker_schedule_init();
#endif
    SERIAL_PORT.begin(921600, SERIAL_8N1, -1, 1);
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
    SERIAL_PORT.setTxBufferSize(512);
#endif
    MAIN_TAG_CONSOLE("build_time: %s", build_time);
    MAIN_TAG_CONSOLE("==== Firmware version %u.%u.%u ====\r\n",
                     FW_VERSION_MAJOR,
                     FW_VERSION_MINOR,
                     FW_VERSION_BUILD);

    /* Init watch dog timers*/
    wdt_enable(WDT_TIMEOUT_VALUE);

#if (defined LED_STATUS_GPIO) && (LED_STATUS_GPIO != -1)
    LEDStatus.setCycleCallbacks(new ESPLedCycleBlinkCallbacks());
    LEDStatus.statusUpdate(ESPLedCycleBlinkCallbacks::BLINK_NORMAL);
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    Ethernet.OnLedStatus(std::bind(&ESPBlinkGPIO::statusUpdate, &LEDStatus, _1));
#endif
    ESPWifi.OnLedStatus(std::bind(&ESPBlinkGPIO::statusUpdate, &LEDStatus, _1));
#endif

    /* Init eeprom system */
    EEPParams.load();

    MAIN_TAG_CONSOLE("LittleFS Init");
    /* Init nand memory file system */
    if (NAND_FS_SYSTEM.begin(true))
    {
        /* List file in nand memory file system */
        // FSHandle.listDir(NAND_FS_SYSTEM, "/", 0);
        MAIN_TAG_CONSOLE("LittleFS Init succeed");
    }

    /* Load system parameters */
    // Always initialize after NAND_FS_SYSTEM.begin();
    ESPConfig.load(&NAND_FS_SYSTEM);

    /* Init rtc of system */
    // Must be load after ESPConfig.load()
    // Because it needs some parameters to configure time zone.
    ESPTime.load();

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
    /* Init sd card */
    if (SDCard.begin()) {
        /* List file in sd card memory file system */
        // FSHandle.listDir(SD_FS_SYSTEM, "/", 0);
        MAIN_TAG_CONSOLE("SD Card Init succeed");
    }
#endif

    /* Update log */
    MAIN_LOG("\r\n");
    MAIN_FUNCTION_LOG("======================================");
    MAIN_FUNCTION_LOG("========== Board Initialize ==========");
    MAIN_FUNCTION_LOG("Firmware build_time: %s", build_time);
    MAIN_FUNCTION_LOG("==== Firmware version %u.%u.%u ====",
                     FW_VERSION_MAJOR,
                     FW_VERSION_MINOR,
                     FW_VERSION_BUILD);

    MAIN_FUNCTION_LOG("SdkVersion    : %lu", ESP.getSdkVersion());
    MAIN_FUNCTION_LOG("CpuFreqMHz    : %lu", ESP.getCpuFreqMHz());
    MAIN_FUNCTION_LOG("SketchSize    : %lu", ESP.getSketchSize());
    MAIN_FUNCTION_LOG("SketchSpace   : %lu", ESP.getFreeSketchSpace());
    MAIN_FUNCTION_LOG("FlashChipSize : %lu", ESP.getFlashChipSize());
    MAIN_FUNCTION_LOG("FlashChipSpeed: %lu", ESP.getFlashChipSpeed());
    MAIN_FUNCTION_LOG("FlashChipMode : %lu", ESP.getFlashChipMode());
    uint32_t NANDtotalBytes = NAND_FS_SYSTEM.totalBytes();
    uint32_t NANDusedBytes = NAND_FS_SYSTEM.usedBytes();
    MAIN_FUNCTION_LOG(TOSTRING(NAND_FS_SYSTEM) " space: %luB/%luB", NANDusedBytes, NANDtotalBytes);
    MAIN_TAG_CONSOLE(TOSTRING(NAND_FS_SYSTEM) " space: %luB/%luB", NANDusedBytes, NANDtotalBytes);
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
    if (SDCard.statusIsOk()) {
        uint64_t SDtotalBytes = SD_FS_SYSTEM.cardSize() / (1024 * 1024);
        uint64_t SDusedBytes = SD_FS_SYSTEM.usedBytes() / (1024ULL * 1024ULL);
        MAIN_FUNCTION_LOG("SD Card Init succeed: %lluMB/%lluMB", SDusedBytes, SDtotalBytes);
    }
    else {
        MAIN_FUNCTION_LOG("Card Card Init Failed");
    }
#endif
    /* Update log reset reason */
    MAIN_TAG_CONSOLE("Reset reason : %s", esp_reset_reason_str().c_str());
    MAIN_FUNCTION_LOG("Reset reason : %s", esp_reset_reason_str().c_str());
    /* Update log wakeup reason */
    MAIN_TAG_CONSOLE("Wakeup reason: %s", esp_wakeup_reason_str().c_str());
    MAIN_FUNCTION_LOG("Wakeup reason: %s", esp_wakeup_reason_str().c_str());

    /**System time update event
     * From Sntp or web
    */
    ESPTime.onTimeLevelUpdate([](uint8_t level, esp_rtc_time_t* rtc){
        CalendarSchedule.begin(0); /* Run without timeout */
    });

    /* Ping service configure */
    if (ESPConfig.pingType(PING_GATEWAY_ID) == ESPPingService::PING_ERROR_RESET &&
        ESPConfig.pingEnable(PING_GATEWAY_ID)) {
        pingGateway.errorResetTimeout(ESPConfig.pingTimeout(PING_GATEWAY_ID));
    }
    if (ESPConfig.pingType(PING_INTERNET_ID) == ESPPingService::PING_ERROR_RESET &&
        ESPConfig.pingEnable(PING_INTERNET_ID)) {
        pingInternet.errorResetTimeout(ESPConfig.pingTimeout(PING_INTERNET_ID));
    }
    /* Wifi Event connection */
    ESPWifi.onConnection([](bool evt, IPAddress gw) {
        if (evt) {
            MAIN_TAG_CONSOLE("Ping service start");
            if (ESPConfig.pingHost(PING_GATEWAY_ID) == "gateway" &&
                ESPConfig.pingEnable(PING_GATEWAY_ID)) 
            {
                pingGateway.begin(gw.toString().c_str(), ESPConfig.pingInterval(PING_GATEWAY_ID));
            }
            if (ESPConfig.pingEnable(PING_INTERNET_ID)) {
                pingInternet.begin(ESPConfig.pingHost(PING_INTERNET_ID).c_str(), ESPConfig.pingInterval(PING_INTERNET_ID));
            }
        }
        else {
            MAIN_TAG_CONSOLE("Ping service stop");
            pingGateway.end();
            pingInternet.end();
        }
    });
    ESPWifi.onOTA([](bool evt){
        if (evt) {
            MAIN_TAG_CONSOLE("Ping service stop");
            pingGateway.pingStop();
            pingInternet.pingStop();
        }
        else {
            MAIN_TAG_CONSOLE("Ping service continue");
            pingGateway.pingContinue();
            pingInternet.pingContinue();
        }
    });
    ESPWifi.OnDDNSService(&EasyDDNS);

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
#if (defined ETH_GPIO_ENABLE) && (ETH_GPIO_ENABLE != -1)
    ETH_GPIO_ENABLE_INIT();
    if (ETH_STATUS_IS_ON()) { Ethernet.enable(); }
    else { Ethernet.disable(); }
#else
    Ethernet.enable();
#endif
    // If the ethernet is enabled, the Wifi will not be allowed to initialize .
    ESPWifi.begin(!Ethernet.isEnable());
    Ethernet.begin();
#else
    // Init wifi and accompanied services
    ESPWifi.begin();
#endif

    // register event callback when a webSocket establish
    wsHandler.OnNewEventConnection([](uint32_t id) {
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
        if (!SDCard.statusIsOk()) {
            // "{"Error":1,"Msg":"The Nho\"}"
            wsHandler.asyncEvents().send("{\"Error\":1,\"Msg\":\"The Nho\"}", "System_alert");
        } 
#endif
    });
    wsHandler.setHandleCallbacks(&WsDataHandler);
    wsHandler.begin();
    webServer.onUrlHandle(new WebServerURLHandle("param_wifi"));
    webServer.begin(&wsHandler);

#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
    factorySysParams.onFactory([]() {
        ESPConfig.setDefault();
#if (defined LED_STATUS_GPIO) && (LED_STATUS_GPIO != -1)
        LEDStatus.statusUpdate(ESPLedCycleBlinkCallbacks::BLINK_FACTORY_SYSTEM_PARAMS);
#endif
        MAIN_TAG_CONSOLE("RESET FACTORY OK");
        MAIN_FUNCTION_LOG("RESET FACTORY OK");
    });

    // handle factory system params by hold button over 5s
    factorySysParams.begin(5000);
#endif

    /**
     * SoftReset event handler
    */
    SOFTReset.onReset([](uint32_t timeout) {
        (void) timeout;
        MAIN_TAG_CONSOLE("onReset handler");
        pingGateway.end();
        pingInternet.end();
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
        if (Ethernet.isEnable()) {
            Ethernet.disable();
            esp_eth_disable();
            esp_eth_deinit();
        }
#endif
    });

    /**
     * Creation a schedule Alarm Clock
     * Implemented Alarm clock reset
    */
    CalendarSchedule.begin();
}

void loop() {
    /* Watch dog timer feed */
    wdt_reset();

    ESPWifi.loop();
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1) && (defined ESP8266)
    Ethernet.loop();
#endif

    if (!ESPWifi.IsOTA()) {
        /* Only handle without OTA processing */
        ticker_schedule_handler();
    }
}