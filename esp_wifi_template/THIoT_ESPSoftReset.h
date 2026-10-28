#ifndef	__ESP_SOFT_RESET_H
#define __ESP_SOFT_RESET_H

#include <Arduino.h>
#include <functional>
#include <FS.h>
#include <Ticker.h>
#include "THIoT_PFTicker.h"

#define RESET_HEAP_ALLOCATE_MAX (1024U * 30) /* The max heap allocate size */
#define RESET_HEAP_FREE_MAX     (1024U * 80) /* The max heap free size */
#define RESET_HEAP_OFFSET_MAX   (1024U * 40) /* The offset between heap current and heap max */
#define RESET_HEAP_OFFSET_MIN   (1024U * 30)  /* The offset between heap current and heap max */
#define RESET_HEAP_TIMEOUT_SEC  40U  /* Timeout to reset when heap reach to limitation */
#define RESET_HEAP_INTERVAL_SEC 5U   /* Interval check heap */
#define RESET_HEAP_START_SEC    90U  /* Timeout to starting check heap limitation*/

typedef std::function<void(uint32_t, uint8_t)> ResetHandler;

typedef struct {
    uint32_t heapFreeMax;
    uint32_t heapAllocateMax;
    uint32_t heap_Offset;
    uint16_t timeoutStart;
    uint8_t timeoutReset;
    uint8_t intervalTick;
    boolean serialTraceConsole;
    boolean fileTraceConsole;
    boolean udpTraceConsole;
    boolean wsTraceConsole;
    boolean editorLock;
    boolean httpLog;
    boolean traceLog;
    boolean monitorTask;
    boolean downloadLimit;
    boolean enable;
} softResetInfo_t;

class SoftResetFileHandler
{
  using File = fs::File;
  using FS = fs::FS;
private:
  FS *_fs;
  softResetInfo_t _softResetInfo;

public:
  typedef enum : uint8_t {
    OK_STATUS = 0,
    ERROR_STATUS,
    UPDATE_STATUS
  } syncStatus_t;

  SoftResetFileHandler(FS &fs);
  ~SoftResetFileHandler();

  void load();
  void save();
  uint8_t sync();
  void setDefault();

  void enableSet(boolean enable) { _softResetInfo.enable = enable; }
  boolean enableGet() { return _softResetInfo.enable; }

  void serialTraceConsoleSet(boolean enable) { _softResetInfo.serialTraceConsole = enable; }
  boolean serialTraceConsoleGet() { return _softResetInfo.serialTraceConsole; }

  void fileTraceConsoleSet(boolean enable) { _softResetInfo.fileTraceConsole = enable; }
  boolean fileTraceConsoleGet() { return _softResetInfo.fileTraceConsole; }

  void udpTraceConsoleSet(boolean enable) { _softResetInfo.udpTraceConsole = enable; }
  boolean udpTraceConsoleGet() { return _softResetInfo.udpTraceConsole; }

  void wsTraceConsoleSet(boolean enable) { _softResetInfo.wsTraceConsole = enable; }
  boolean wsTraceConsoleGet() { return _softResetInfo.wsTraceConsole; }

  void httpLogSet(boolean enable) { _softResetInfo.httpLog = enable; }
  boolean httpLogGet() { return _softResetInfo.httpLog; }

  void traceLogSet(boolean enable) { _softResetInfo.traceLog = enable; }
  boolean traceLogGet() { return _softResetInfo.traceLog; }

  void monitorTaskSet(boolean enable) { _softResetInfo.monitorTask = enable; }
  boolean monitorTaskGet() { return _softResetInfo.monitorTask; }

  void editorLockSet(boolean enable) { _softResetInfo.editorLock = enable; }
  boolean editorLockGet() { return _softResetInfo.editorLock; }

  void downloadLimitSet(boolean enable) { _softResetInfo.downloadLimit = enable; }
  boolean downloadLimitGet() { return _softResetInfo.downloadLimit; }

  void heap_OffsetSet(uint32_t heap_Offset) { _softResetInfo.heap_Offset = heap_Offset; }
  uint32_t heap_OffsetGet() { return _softResetInfo.heap_Offset; }

  void heapFreeMaxSet(uint32_t heapFreeMax) { _softResetInfo.heapFreeMax = heapFreeMax; }
  uint32_t heapFreeMaxGet() { return _softResetInfo.heapFreeMax; }

  void heapAllocateMaxSet(uint32_t heapAllocateMax) { _softResetInfo.heapAllocateMax = heapAllocateMax; }
  uint32_t heapAllocateMaxGet() { return _softResetInfo.heapAllocateMax; }

  void timeoutStartSet(uint16_t timeoutStart) { _softResetInfo.timeoutStart = timeoutStart; }
  uint16_t timeoutStartGet() { return _softResetInfo.timeoutStart; }

  void timeoutResetSet(uint8_t timeoutReset) { _softResetInfo.timeoutReset = timeoutReset; }
  uint8_t timeoutResetGet() { return _softResetInfo.timeoutReset; }

  void intervalTickSet(uint8_t intervalTick) { _softResetInfo.intervalTick = intervalTick; }
  uint8_t intervalTickGet() { return _softResetInfo.intervalTick; }
};

class ESPSoftReset : public SoftResetFileHandler
{
public:
    typedef enum : uint8_t {
        HOME_PAGE_TYPE = 0,
        I2C_DEVICE_ADDR_SETTING_TYPE,
        DEVICE_INFO_NAME_TYPE,
        DEVICE_AUTH_SETTING_TYPE,
        STA_INFO_SETTING_TYPE,
        AP_INFO_SETTING_TYPE,
        OTA_UPDATE_TYPE,
        HTTP_API_INFO_SETTING_TYPE,
        SCHEDULE_DAILY_TYPE,
        SCHEDULE_WEEKLY_TYPE,
        WIFI_RECONNECT_TYPE,
        ETH_DISCONNECT_TYPE,
        PING_GATEWAY_FAIL_TYPE,
        SD_FORMAT_TYPE,
        HTTP_API_REQUEST_ERROR,
        HEAP_MEM_LOW_TYPE,
        LOG_5T_SETTING_TYPE,
        SNTP_TIMEOUT_TYPE,
        LOG_TRACE_QUEUE_TYPE,
        I2C_SYNC_TYPE,
        PUMP_LOG_TYPE,
        RESET_REASON_TYPE_MAX
    } resetSource_t;
private:
    Ticker _resetTick;
    Ticker _heapResetTick;
    ResetHandler _resetFunc = nullptr;
    resetSource_t _reasonType;
    uint32_t _heapMin;
    uint32_t _heapMax;
    uint16_t _startCounter;
    uint8_t _resetCounter;
    
public:
    ESPSoftReset();
    ~ESPSoftReset() {}
    void begin();
    boolean enable(uint32_t timeout, resetSource_t type = HOME_PAGE_TYPE);
    void onReset(ResetHandler handler) { _resetFunc = handler; }
    void heapMonitor();
};

extern ESPSoftReset SOFTReset;

#endif // __ESP_SOFT_RESET_H
