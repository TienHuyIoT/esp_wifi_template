#include <Esp.h>
#include <ArduinoJson.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define RESET_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[RESET]", __VA_ARGS__)

#define RESET_TAG_LOG(...) FS_TAG_LOGI("[RESET]", __VA_ARGS__)

const char * const reset_reason_type[] PROGMEM {
/*00*/ "HOME_PAGE_TYPE",
/*01*/ "I2C_DEVICE_ADDR_SETTING_TYPE",
/*02*/ "DEVICE_INFO_NAME_TYPE",
/*03*/ "DEVICE_AUTH_SETTING_TYPE",
/*04*/ "STA_INFO_SETTING_TYPE",
/*05*/ "AP_INFO_SETTING_TYPE",
/*06*/ "OTA_UPDATE_TYPE",
/*07*/ "HTTP_API_INFO_SETTING_TYPE",
/*08*/ "SCHEDULE_DAILY_TYPE",
/*09*/ "SCHEDULE_WEEKLY_TYPE",
/*10*/ "WIFI_RECONNECT_TYPE",
/*11*/ "ETH_DISCONNECT_TYPE"
};

ESPSoftReset::ESPSoftReset(/* args */) {}

boolean ESPSoftReset::enable(uint32_t timeout, resetSource_t type)
{
    RESET_TAG_CONSOLE("enable after %ums", timeout);
    if (_resetFunc) {
        _resetFunc(timeout);
    }

    _reasonType = type;
    ticker_once_ms(&_resetTick, timeout, [](void* arg) {
        ESPSoftReset* handler = (ESPSoftReset*)arg;
        RESET_TAG_LOG("Reason source: %s", reset_reason_type[(uint8_t)handler->_reasonType]);
        RESET_TAG_CONSOLE("Reason source: %s", reset_reason_type[(uint8_t)handler->_reasonType]);
        ESPTime.saveToFileSystem();
        RESET_TAG_CONSOLE("\tRebooting...");
        SERIAL_FLUSH();
        delay(150);
        ESP.restart();
    }, this);

    return true;
}

ESPSoftReset SOFTReset;
