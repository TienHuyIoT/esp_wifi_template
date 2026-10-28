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
#include <lwip/tcpip.h> // tcpip_callback()
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
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFConsoleTrace.h"
#include "THIoT_ESPTaskMonitor.h"
#include "THIoT_ESPTaskHelper.h"
#include "THIoT_APPCoreDump.h"
#include "THIoT_ESPOTAManager.h"

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
/* Timeout check used to stop the serial debug stream (see the console trace setup) */
#define SERIAL_TRACE_CONSOLE_TIMEOUT    60U
ticker_function_handle_t serialTraceConsoleTicker;

/* Handle of the lwIP tcpip task, used by the task monitor.  It is captured from
 * inside the lwIP task itself (see the tcpip_callback() in setup()). */
TaskHandle_t _lwIP_TaskHandle;
static void lwipTaskHandleCapture(void *ctx) {
    (void) ctx;
    _lwIP_TaskHandle = xTaskGetCurrentTaskHandle();
}
#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
FactoryButton factorySysParams(FACTORY_INPUT_PIN);
#endif
#if (defined LED_STATUS_GPIO) && (LED_STATUS_GPIO != -1)
ESPBlinkGPIO LEDStatus(HIGH, LED_STATUS_GPIO); /* v3 signature: (stable/off level, gpio) */
#endif

void setup() {
    using namespace std::placeholders;
#if (MUTEX_PLATFORM_TICKER == 1)
    ticker_schedule_init();
#endif
    SERIAL_PORT.begin(SERIAL_BAUDRATE, SERIAL_8N1, -1, 1);
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
    SERIAL_PORT.setTxBufferSize(512);
#endif
    SERIAL_PORT.printf("\n\n(^-^)\n\n");

    /* Console trace task (serial + file + WebSocket) - must be started before the
     * first log line, otherwise the early boot logs are lost. */
#if (gConsoleDebugLevel > 0)
    consoleTraceTaskBegin();
#endif
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    CONSOLE_TRACE_STREAM.beginSerial(&SERIAL_PORT);
#endif

    /* Shared log-trace task (writes the FS log records) */
    logTraceTaskBegin();

    /* OTA validation state of the previous boot (power on reset handler) */
#if (OTA_CONFIRM_FIRMWARE_VALIDATION == 1)
    esp_ota_power_on_reset_handler();
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

    /* Confirm the running image so the bootloader does not roll it back */
    esp_ota_confirm_image_rollback();

#if defined(CORE_DUMP_TRACE_RECORD) && (CORE_DUMP_TRACE_RECORD == 1)
    /* The file system is up at this point: record a pending core dump (if any)
     * and reboot so the crash is not overwritten by the next one. */
    if (gAppCoreDump.record()) {
        MAIN_TAG_CONSOLE("Restart system after core dump record");
        delay(100);
        esp_restart();
        return;
    }
#endif

    /* SoftReset configuration: heap guard + trace/console enable flags stored in
     * /SOFT_RESET.TXT (load() must run after the FS is mounted). */
    SOFTReset.load();
    SD_FS_LOG.enableSet(SOFTReset.traceLogGet());
    ESP_FS_LOG.enable(); // Always enable internal log
#if (CONSOLE_TRACE_WS_STREAM == 1)
    CONSOLE_TRACE_STREAM.wsConsoleSet(SOFTReset.wsTraceConsoleGet());
#endif
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    ticker_once(&serialTraceConsoleTicker, SERIAL_TRACE_CONSOLE_TIMEOUT, [](void *arg) {
#if (CONSOLE_TRACE_FILE_STREAM == 1)
        if (SDCard.statusIsOk() && SOFTReset.fileTraceConsoleGet()) {
            MAIN_TAG_CONSOLE("File recorder Debug Enable");
            CONSOLE_TRACE_STREAM.beginFs(SD_FS_SYSTEM, CONSOLE_TRACE_FILE_PATH, CONSOLE_EXTERNAL_MEMORY_SIZE_LIMITED);
        } else {
            MAIN_TAG_CONSOLE("File recorder Debug disable");
        }
#endif
        // should be called latest in case of serial disable
        if (!SOFTReset.serialTraceConsoleGet()) {
            MAIN_TAG_CONSOLE("Disable Serial Debug");
            CONSOLE_TRACE_STREAM.consoleSerialStop();
        } else {
            MAIN_TAG_CONSOLE("Serial Debug Enable");
        }
        // Check if console trace stream is enable
        if (!CONSOLE_TRACE_STREAM.isEnable()) {
            MAIN_TAG_CONSOLE("Console Trace Stream is disable");
            consoleTraceTaskEnd();
        }
    }, nullptr);
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
    ESPWifi.onOTA([](int evt){
        if (evt == ESPWifiHandle::OTA_START) {
            MAIN_TAG_CONSOLE("Ping service stop");
            pingGateway.pingStop();
            pingInternet.pingStop();
        }
        else if (evt == ESPWifiHandle::OTA_SUCCESS) {
            MAIN_TAG_CONSOLE("OTA succeed");
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
    Ethernet.disable();
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
    /* Editor write protection + download limit, taken from the soft-reset config */
    webServer.editorLock(SOFTReset.editorLockGet());
    webServer.editorDownloadLimited(SOFTReset.downloadLimitGet());

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
    SOFTReset.onReset([](uint32_t timeout, uint8_t type) {
        MAIN_TAG_CONSOLE("onReset handler: timeout = %u, type = %u", timeout, type);
        pingGateway.end();
        pingInternet.end();
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
        if (Ethernet.isEnable()) {
            Ethernet.disable();
            /* esp_eth_disable()/esp_eth_deinit() were removed from esp_eth.h in IDF 4.x
               (legacy API), so they cannot be called any more.  ETH.end() performs the
               same teardown: esp_eth_stop() + esp_eth_driver_uninstall(). */
            ETH.end();
        }
#endif
    });

    /* Start the heap guard + the reset timers (after load() has read the config) */
    SOFTReset.begin();

    /* Capture the lwIP tcpip task handle (runs in the lwIP task context) */
    tcpip_callback(lwipTaskHandleCapture, NULL);

    /* Task monitor: watch the stack high-water marks of the registered tasks */
    if (SOFTReset.monitorTaskGet()) {
        ESPTaskMonitor.begin();
    }

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