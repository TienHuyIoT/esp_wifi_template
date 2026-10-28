#include <Esp.h>
#include <ArduinoJson.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_APPConfig.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPResetReason.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define RESET_TAG_CONSOLE(...)      //CONSOLE_TAG_LOGI("[RESET]", __VA_ARGS__)
#define RESET_TAG_CONSOLE_RS(...)   CONSOLE_TAG_LOGI("[RESET]", __VA_ARGS__)

#define RESET_TAG_LOG(...)      FS_TAG_LOGI("[RESET]", __VA_ARGS__)
#define RESET_TAG_ESP_LOG(...)  ESP_FS_TAG_LOGI("[RESET]", __VA_ARGS__)

#define SOFT_RESET_FILE_PATH ((const char*)(APP_FOLDER_CFG_PATH "/rst_cfg.txt"))

const char softResetDefault[] PROGMEM = R"=====({
    "heapFreeMax": 81920,
    "heapAllocateMax": 30720,
    "heap_Offset": 40960,
    "timeoutReset": 40,
    "intervalTick": 5,
    "timeoutStart": 90,
    "serialTraceConsole": false,
    "fileTraceConsole": false,
    "udpTraceConsole": false,
    "wsTraceConsole": false,
    "editorLock": true,
    "httpLog": false,
    "traceLog": true,
    "monitorTask": false,
    "downloadLimit": false,
    "enable": true
})=====" ;

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
/*11*/ "ETH_DISCONNECT_TYPE",
/*12*/ "PING_GATEWAY_FAIL_TYPE",
/*13*/ "SD_FORMAT_TYPE",
/*14*/ "HTTP_API_REQUEST_ERROR",
/*15*/ "HEAP_MEM_LOW_TYPE",
/*16*/ "LOG_5T_SETTING_TYPE",
/*17*/ "SNTP_TIMEOUT_TYPE",
/*18*/ "LOG_TRACE_QUEUE_TYPE",
/*19*/ "I2C_SYNC_TYPE",
/*20*/ "PUMP_LOG_TYPE",
/*21*/ "RESET_REASON_TYPE_MAX"
};

ESPSoftReset::ESPSoftReset(/* args */)
:SoftResetFileHandler(NAND_FS_SYSTEM) {
    _heapMin = 0;
    _heapMax = 0;
    _startCounter = 0;
    _resetCounter = 0;
    _reasonType = RESET_REASON_TYPE_MAX;
}

void ESPSoftReset::begin() {
    if (enableGet()) {
        RESET_TAG_CONSOLE("Enable _heapResetTick");
        _heapResetTick.attach<void*>(intervalTickGet(), [](void *arg){
            ESPSoftReset *handler = (ESPSoftReset*)arg;
            handler->heapMonitor();
        }, this);
    } else {
        RESET_TAG_CONSOLE("Disable _heapResetTick");
    }
}

boolean ESPSoftReset::enable(uint32_t timeout, resetSource_t type)
{
    if (type >= RESET_REASON_TYPE_MAX || _reasonType == type) {
        RESET_TAG_CONSOLE_RS("Already enabled, type = %u", type);
        return false;
    }
    _reasonType = type;

    RESET_TAG_CONSOLE_RS("enable after %ums, type = %u", timeout, type);
    
    if (SD_FORMAT_TYPE == type) {
        /* Due to sd format after reset, so the log has to be stored inside internal memory */
        RESET_TAG_ESP_LOG("Reason source: %s", reset_reason_type[(uint8_t)type]);
    } else if (OTA_UPDATE_TYPE == type) {
        esp_reset_reason_force_as_PowerON();
        RESET_TAG_LOG("Reason source: %s", reset_reason_type[(uint8_t)type]);
    } else {
        RESET_TAG_LOG("Reason source: %s", reset_reason_type[(uint8_t)type]);
    }
    RESET_TAG_CONSOLE_RS("Reason source: %s", reset_reason_type[(uint8_t)type]);
    RESET_TAG_CONSOLE_RS("Timeout Reset: %ums", timeout);
    _resetTick.attach_ms<void*>(timeout, [](void* arg) 
    {
        RESET_TAG_CONSOLE_RS("\tRebooting...");
        CONSOLE_FLUSH();

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
        if (Ethernet.isEnable()) {
            Ethernet.disable();
#if ESP_IDF_VERSION_MAJOR > 3
            ETH.end();
#else
            esp_eth_disable();
            esp_eth_deinit();
#endif
        }
#endif
        delay(100);
        ESPTime.timeSystemUpdate();
        ESP.restart();
    }, this);

    // Have to place this after the ticker is set
    // Due to if the reset reason is set before the ticker, it will not be able to call the reset function
    // Example: if the reset reason is set from i2c task, then the reset function will be stoped due to the i2c task is deleted in the callback function.
    if (_resetFunc) {
        _resetFunc(timeout, type);
    }
    return true;
}

void ESPSoftReset::heapMonitor() {
    if (_startCounter < timeoutStartGet()) {
        _startCounter += intervalTickGet();
        return;
    }

    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap > _heapMax) {
        _heapMax = freeHeap;
    }
    if (freeHeap < _heapMin || _heapMin == 0) {
        _heapMin = freeHeap;
    }

    uint32_t heap_Offset = _heapMax - freeHeap;
    size_t heapAllocateMax = ESP.getMaxAllocHeap();
    RESET_TAG_CONSOLE("freeHeap = %u, _heapMax = %u, _heapMin = %u, heap_Offset = %u", freeHeap, _heapMax, _heapMin, heap_Offset);
    if (heap_Offset > heap_OffsetGet() || heapAllocateMax < heapAllocateMaxGet() || freeHeap < heapFreeMaxGet()) {
        _resetCounter += intervalTickGet();
        RESET_TAG_CONSOLE("_resetCounter = %u", _resetCounter);
        if (_resetCounter >= timeoutResetGet()) {
            RESET_TAG_LOG("freeHeap = %u(%u), _heapMax = %u, _heapMin = %u, heap_Offset = %u(%u), heapAllocateMax = %u(%u)",
                          freeHeap, heapFreeMaxGet(),
                          _heapMax,
                          _heapMin,
                          heap_Offset, heap_OffsetGet(),
                          heapAllocateMax, heapAllocateMaxGet());
            enable(500, HEAP_MEM_LOW_TYPE);
        }
    } else {
        _resetCounter = 0;
    }
}

ESPSoftReset SOFTReset;

/* SoftResetFileHandler object handle -------------------------------------------------*/
SoftResetFileHandler::SoftResetFileHandler(FS &fs) : _fs(&fs) {
    _softResetInfo.heapFreeMax  = RESET_HEAP_FREE_MAX;
    _softResetInfo.heapAllocateMax  = RESET_HEAP_ALLOCATE_MAX;
    _softResetInfo.heap_Offset  = RESET_HEAP_OFFSET_MAX;
    _softResetInfo.timeoutReset = RESET_HEAP_TIMEOUT_SEC;
    _softResetInfo.intervalTick = RESET_HEAP_INTERVAL_SEC;
    _softResetInfo.timeoutStart = RESET_HEAP_START_SEC;
    _softResetInfo.serialTraceConsole = false;
    _softResetInfo.fileTraceConsole = false;
    _softResetInfo.udpTraceConsole  = false;
    _softResetInfo.wsTraceConsole  = false;
    _softResetInfo.editorLock       = true;
    _softResetInfo.httpLog          = false;
    _softResetInfo.traceLog         = true;
    _softResetInfo.monitorTask      = false;
    _softResetInfo.downloadLimit    = false;
    _softResetInfo.enable           = true;
}

SoftResetFileHandler::~SoftResetFileHandler() {}

void SoftResetFileHandler::load() {
    uint8_t status = sync();
    if (UPDATE_STATUS == status) {
        save();
    }
}

void SoftResetFileHandler::save() {
    File fs_handle;

    const size_t capacity = JSON_OBJECT_SIZE(20);
    DynamicJsonBuffer djBuffer(capacity);
    JsonObject &root = djBuffer.createObject();
    root["heapFreeMax"]  = _softResetInfo.heapFreeMax;
    root["heapAllocateMax"]  = _softResetInfo.heapAllocateMax;
    root["heap_Offset"]  = _softResetInfo.heap_Offset;
    root["timeoutReset"] = _softResetInfo.timeoutReset;
    root["intervalTick"] = _softResetInfo.intervalTick;
    root["timeoutStart"] = _softResetInfo.timeoutStart;
    root["serialTraceConsole"] = _softResetInfo.serialTraceConsole;
    root["fileTraceConsole"] = _softResetInfo.fileTraceConsole;
    root["udpTraceConsole"] = _softResetInfo.udpTraceConsole;
    root["wsTraceConsole"] = _softResetInfo.wsTraceConsole;
    root["editorLock"] = _softResetInfo.editorLock;
    root["httpLog"] = _softResetInfo.httpLog;
    root["traceLog"] = _softResetInfo.traceLog;
    root["monitorTask"] = _softResetInfo.monitorTask;
    root["downloadLimit"] = _softResetInfo.downloadLimit;
    root["enable"]       = _softResetInfo.enable;
    fs_handle = _fs->open(SOFT_RESET_FILE_PATH, "w");
    if (!fs_handle) {
        RESET_TAG_CONSOLE("Failed to open file %s for writing", SOFT_RESET_FILE_PATH);
        return;
    }
    root.prettyPrintTo<File>(fs_handle);
    fs_handle.close();
    RESET_TAG_CONSOLE("save succeed!");
}

uint8_t SoftResetFileHandler::sync() {
    File fs_handle;
    uint8_t status = OK_STATUS;

    if (!_fs->exists(APP_FOLDER_CFG_PATH)) {
        RESET_TAG_CONSOLE("mkdir %s", APP_FOLDER_CFG_PATH);
        _fs->mkdir(APP_FOLDER_CFG_PATH);
    }

    if (!_fs->exists(SOFT_RESET_FILE_PATH)) {
        fs_handle = _fs->open(SOFT_RESET_FILE_PATH, "w");
        if (!fs_handle) {
            RESET_TAG_CONSOLE("Failed to open file %s for writing", SOFT_RESET_FILE_PATH);
            return ERROR_STATUS;
        }
        fs_handle.printf_P(softResetDefault);
        fs_handle.close();
        RESET_TAG_CONSOLE("Init data succeed!");
    }

    fs_handle = _fs->open(SOFT_RESET_FILE_PATH, "r");
    if (!fs_handle) {
        RESET_TAG_CONSOLE("Failed to open file %s for reading", SOFT_RESET_FILE_PATH);
        return ERROR_STATUS;
    }

    const size_t capacity = JSON_OBJECT_SIZE(20) + 150;
    DynamicJsonBuffer djBuffer(capacity);
    JsonObject &root = djBuffer.parseObject(fs_handle);
    fs_handle.close();

    if (!root.success()) {
        RESET_TAG_CONSOLE("JSON parsing failed!");
        fs_handle = _fs->open(SOFT_RESET_FILE_PATH, "w");
        if (!fs_handle) {
            RESET_TAG_CONSOLE("Failed to open file %s for writing", SOFT_RESET_FILE_PATH);
            return ERROR_STATUS;
        }
        fs_handle.printf_P(softResetDefault);
        fs_handle.close();
        return ERROR_STATUS;
    }

    if ( root["heapFreeMax"].success()) {
        _softResetInfo.heapFreeMax = root["heapFreeMax"];
    } else {
        status = UPDATE_STATUS;
    }

    if ( root["heapAllocateMax"].success()) {
        _softResetInfo.heapAllocateMax = root["heapAllocateMax"];
    } else {
        status = UPDATE_STATUS;
    }
    _softResetInfo.heap_Offset  = root["heap_Offset"];
    _softResetInfo.timeoutReset = root["timeoutReset"];
    _softResetInfo.intervalTick = root["intervalTick"];
    _softResetInfo.timeoutStart = root["timeoutStart"];
    _softResetInfo.enable       = root["enable"];
    if (root["editorLock"].success()) _softResetInfo.editorLock = root["editorLock"];
    if (root["httpLog"].success()) _softResetInfo.httpLog       = root["httpLog"];
    if (root["traceLog"].success()) {_softResetInfo.traceLog    = root["traceLog"];}
    if (root["downloadLimit"].success()) {_softResetInfo.downloadLimit = root["downloadLimit"];}
    else {status = UPDATE_STATUS;}
   
    if (_softResetInfo.heap_Offset < RESET_HEAP_OFFSET_MIN) {
        _softResetInfo.heap_Offset = RESET_HEAP_OFFSET_MIN;
        status = UPDATE_STATUS;
    }

    if (_softResetInfo.timeoutReset < 10) {
        _softResetInfo.timeoutReset = 10;
    }

    if (_softResetInfo.intervalTick == 0) {
        _softResetInfo.intervalTick = 1;
    }

    if (_softResetInfo.timeoutStart < 60) {
        _softResetInfo.timeoutStart = 60;
    }

    if ( root["monitorTask"].success()) {
        _softResetInfo.monitorTask = root["monitorTask"];
    } else {
        status = UPDATE_STATUS;
    }

    if ( root["serialTraceConsole"].success()) {
        _softResetInfo.serialTraceConsole = root["serialTraceConsole"];
    } else {
        status = UPDATE_STATUS;
    }

    if ( root["fileTraceConsole"].success()) {
        _softResetInfo.fileTraceConsole = root["fileTraceConsole"];
    } else {
        status = UPDATE_STATUS;
    }

    if ( root["udpTraceConsole"].success()) {
        _softResetInfo.udpTraceConsole = root["udpTraceConsole"];
    } else {
        status = UPDATE_STATUS;
    }

    if ( root["wsTraceConsole"].success()) {
        _softResetInfo.wsTraceConsole = root["wsTraceConsole"];
    } else {
        status = UPDATE_STATUS;
    }

    RESET_TAG_CONSOLE("heapFreeMax: %u", _softResetInfo.heapFreeMax);
    RESET_TAG_CONSOLE("heapAllocateMax: %u", _softResetInfo.heapAllocateMax);
    RESET_TAG_CONSOLE("heap_Offset: %u", _softResetInfo.heap_Offset);
    RESET_TAG_CONSOLE("timeoutReset: %u", _softResetInfo.timeoutReset);
    RESET_TAG_CONSOLE("intervalTick: %u", _softResetInfo.intervalTick);
    RESET_TAG_CONSOLE("timeoutStart: %u", _softResetInfo.timeoutStart);
    RESET_TAG_CONSOLE("serialTraceConsole: %u", _softResetInfo.serialTraceConsole);
    RESET_TAG_CONSOLE("fileTraceConsole: %u", _softResetInfo.fileTraceConsole);
    RESET_TAG_CONSOLE("udpTraceConsole: %u", _softResetInfo.udpTraceConsole);
    RESET_TAG_CONSOLE("wsTraceConsole: %u", _softResetInfo.wsTraceConsole);
    RESET_TAG_CONSOLE("editorLock: %u", _softResetInfo.editorLock);
    RESET_TAG_CONSOLE("httpLog: %u", _softResetInfo.httpLog);
    RESET_TAG_CONSOLE("traceLog: %u", _softResetInfo.traceLog);
    RESET_TAG_CONSOLE("monitorTask: %u", _softResetInfo.monitorTask);
    RESET_TAG_CONSOLE("downloadLimit: %u", _softResetInfo.downloadLimit);
    RESET_TAG_CONSOLE("enable: %u", _softResetInfo.enable);
    return status;
}

void SoftResetFileHandler::setDefault(void) {
    if (_fs->remove(SOFT_RESET_FILE_PATH)) {
        RESET_TAG_CONSOLE("- %s file is deleted\r\n", SOFT_RESET_FILE_PATH);
    }
    else {
        RESET_TAG_CONSOLE("- %s delete failed\r\n", SOFT_RESET_FILE_PATH);
    }
}
