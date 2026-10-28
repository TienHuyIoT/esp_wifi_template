#ifndef __ESP_LOG_TRACE_H
#define __ESP_LOG_TRACE_H

#include <Arduino.h>
#include <Ticker.h>
#include <FS.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPMessageQueue.h"
#include "THIoT_ESPTimeType.h"

#define LOG_INTERNAL_MEMORY_SIZE_LIMITED    (1024 * 100)
#define LOG_TRACE_SD_SIZE_LIMITED           (1024 * 1024 * 2)   // 2MByte
#define LOG_TRACE_EDITOR_VIEW_SIZE          (1024 * 128)        // 128KByte
#define LOG_TRACE_SD_FILE_PATH              ((const char*)("/LOG_TRACE.TXT"))
#define LOG_TRACE_ESP_FILE_PATH             ((const char*)("/ESP_LOG.TXT"))

class ESPLogTrace;
#define LOG_TRACE_FILE_MUTEX_TIMEOUT_MS     30000U
#define LOG_TRACE_FILE_MUTEX_WAIT_MIN       1000U

#define LOG_TRACE_QUEUE_TASK_SIZE           32U
#define LOG_TRACE_RECEIVE_QUEUE_TIMEOUT_MS  10000U
#define LOG_TRACE_SEND_QUEUE_TIMEOUT_MS     30000U // In case of queue full, it will wait for 30 seconds to send data
#define LOG_TRACE_SEND_QUEUE_WAIT           (60000U * 3U) // 3 minutes
#define LOG_TRACE_SEND_QUEUE_NO_WAIT        0U
#define LOG_TRACE_SEND_QUEUE_WAIT_MIN       100U
#define PUMP_LOG_TOTAL_PATH_LENGTH_MAX      30U
#define PUMP_LOG_FOLDER_PATH_LENGTH         15U
extern ESPLogTrace PUMP_LOG_TOTAL_FS_RECORD;
String getPumpLogTotalPath(FS *fs, esp_rtc_time_t *rtc);

void logTraceTaskBegin();

class ESPLogTrace : public Print
{
    using File = fs::File;
    using FS = fs::FS;
private:
    FS *_fs;
    String _fileName;
    String _header;     /* Header should be without \r\n */
    size_t _limitSize;
    size_t _fileSize;           /* size of the file named by _fileSizeName */
    String _fileSizeName;       /* file that _fileSize describes */
    TickType_t _timeoutQueueRx;
    TickType_t _timeoutQueueTx;
    TickType_t _timeoutMutexFile;
    int _queueSize;
    uint8_t _fsType;
    bool _enable;
    bool _timeLogOn;
    bool _runtimeOn;
    /* Protects the public state (_fileName, _fs, _fsType, _limitSize, _enable,
     * _fileSize) shared by every task that uses this object.
     * Lock order: state mutex -> file lock, and the state mutex is never held while
     * waiting for a file lock (see _addLogTo), otherwise the FS editor, which holds
     * a file lock and then logs, would deadlock against a logging task. */
    SemaphoreHandle_t _stateMutex;
    bool _stateLock();
    void _stateUnlock();
    void _storeProgress(const char *fileName, size_t fileSize);
    bool assertOverSize(FS *fs, const char *fileName, size_t fileSize);
    String backupName() {
        String nameBk = _fileName;
        nameBk.replace(".", "_BK.");
        return nameBk;
    }
    /* Optional per object log task (taskHandle = true in the constructor). When it is
     * not used, every instance writes through the shared log-trace task queue, and the
     * target file of a record must then travel with the record (writeTo/printfTo)
     * instead of by changing _fileName. */
    ESPMessageQueue *_task_queue;
    TaskHandle_t _task_handle;
    StackType_t  *_xStack;
    StaticTask_t *_xTaskBuffer;
    SemaphoreHandle_t _task_mutex;
    /* Option (b): cleared-delete signal used by _startTask() to wait for the
     * deferred delete ticker of this object's own task. */
    SemaphoreHandle_t _task_delete_sem;
    Ticker _task_tick;
    String _queueName;
    bool _task_running;
    bool _taskHandleEnabled;
    bool _startTask();
    bool _queueInit();
    static void _task(void *pvParameters);
    void _loop();

    size_t _addLog(const uint8_t *buffer, size_t length);
    /* Write [buffer] to [fileName]: the record carries its own target file. */
    size_t _addLogTo(const char *fileName, const uint8_t *buffer, size_t length);
    /* Queue one record; [fileName] travels with the item so the log task writes to
     * the file the record was created for. */
    size_t _enqueue(const char *fileName, const uint8_t *buffer, size_t size);
public:
    ESPLogTrace(FS &fs = NAND_FS_SYSTEM,
                const char *fileName = LOG_TRACE_SD_FILE_PATH,
                size_t limitSize = LOG_INTERNAL_MEMORY_SIZE_LIMITED,
                const char *header = nullptr,
                bool timeLogOn = true,
                bool runtimeOn = false,
                bool taskHandle = false,                                        // true: this object owns its log task
                TickType_t timeoutQueueRx = LOG_TRACE_RECEIVE_QUEUE_TIMEOUT_MS, // receive timeout of the own log task
                TickType_t timeoutQueueTx = LOG_TRACE_SEND_QUEUE_TIMEOUT_MS,    // queue send timeout
                TickType_t timeoutMutexFile = LOG_TRACE_FILE_MUTEX_TIMEOUT_MS,  // timeout when taking the file lock
                int queueSize = LOG_TRACE_QUEUE_TASK_SIZE);                     // size of the own log task queue
    ~ESPLogTrace();

    void fsHandleSet(FS &fs);
    void fileNameSet(const char *fileName);
    String fileNameGet();
    void limitSizeSet(size_t limitSize);
    /* Brief: Hold/release the state of this object, so a caller can compose several
     *        state operations. Do not wrap a file write with it: the write waits for
     *        the file lock, which would invert the lock order. */
    void enterCritical();
    void exitCritical();
    size_t write(uint8_t c) override { write(&c, 1); return 1; }
    size_t write(const uint8_t *buffer, size_t size) override;
    /* Brief: Write to exactly [fileName] without touching the state of the object, so
     *        two tasks may write to different day files of the same object without
     *        racing on _fileName. [format] is a plain (RAM readable) format string. */
    size_t writeTo(const char *fileName, const uint8_t *buffer, size_t size);
    size_t printfTo(const char *fileName, const char *format, ...);
    void flush();
    void clean();
    void headerFile(const char *header);
    void enableSet(bool enable);
    void enable();
    void disable();
    bool isFinished();
    size_t size();
    static size_t _s_eventDataHandle(void *arg, const uint8_t *log, size_t length);
    static size_t _s_eventDataHandleTo(void *arg, const char *fileName, const uint8_t *log, size_t length);
};

/* External log */
extern ESPLogTrace SD_FS_LOG;

#define FS_PORT SD_FS_LOG
#define FS_LOG_CLEAN() FS_PORT.clean()
#define FS_FLUSH() FS_PORT.flush()
#define FS_PRINTF(f_, ...)  FS_PORT.printf_P(PSTR("\xff" f_), ##__VA_ARGS__)
#define FS_PRINTFLF(f_, ...)  FS_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)
#define FS_TAG_PRINTF(tag, f_, ...)  FS_PORT.printf_P(PSTR(tag f_), ##__VA_ARGS__)
#define FS_FUNCTION_PRINTF(f_, ...)  FS_PORT.printf_P(PSTR(f_), __FUNCTION__, ##__VA_ARGS__)
#define FS_FUNCTION_PRINTFLF(f_, ...)  FS_PORT.printf_P(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)

#define g_fsLevel 4

#define FS_LOGE(...) do{if(g_fsLevel > 0) {FS_PRINTF(__VA_ARGS__);}}while(0)
#define FS_LOGW(...) do{if(g_fsLevel > 1) {FS_PRINTF(__VA_ARGS__);}}while(0)
#define FS_LOGI(...) do{if(g_fsLevel > 2) {FS_PRINTF(__VA_ARGS__);}}while(0)
#define FS_LOGD(...) do{if(g_fsLevel > 3) {FS_PRINTF(__VA_ARGS__);}}while(0)
#define FS_LOGV(...) do{if(g_fsLevel > 4) {FS_PRINTF(__VA_ARGS__);}}while(0)

#define FS_TAG_LOGE(x, ...) do{if(g_fsLevel > 0) {FS_PRINTFLF("E " x ": " __VA_ARGS__);}}while(0)
#define FS_TAG_LOGW(x, ...) do{if(g_fsLevel > 1) {FS_PRINTFLF("W " x ": " __VA_ARGS__);}}while(0)
#define FS_TAG_LOGI(x, ...) do{if(g_fsLevel > 2) {FS_PRINTFLF("I " x ": " __VA_ARGS__);}}while(0)
#define FS_TAG_LOGD(x, ...) do{if(g_fsLevel > 3) {FS_PRINTFLF("D " x ": " __VA_ARGS__);}}while(0)
#define FS_TAG_LOGV(x, ...) do{if(g_fsLevel > 4) {FS_PRINTFLF("V " x ": " __VA_ARGS__);}}while(0)

#if (g_fsLevel > 0)
#define FS_FUNCTION_IN(...) do{FS_FUNCTION_PRINTFLF("FUNCTION [%s] IN " __VA_ARGS__);}while(0)
#define FS_FUNCTION_OUT(...) do{FS_FUNCTION_PRINTFLF("FUNCTION [%s] OUT " __VA_ARGS__);}while(0)
#define FS_FUNCTION_LOGI(...) do{FS_FUNCTION_PRINTFLF("I: FUNCTION [%s] " __VA_ARGS__);}while(0)
#define FS_FUNCTION_TAG_LOGI(x, ...) do{FS_FUNCTION_PRINTFLF("I " x ": FUNCTION [%s] " __VA_ARGS__);}while(0)
#else
#define FS_FUNCTION_IN(...)
#define FS_FUNCTION_OUT(...)
#define FS_FUNCTION_LOGI(...)
#define FS_FUNCTION_TAG_LOGI(x, ...)
#endif

/* Internal log */
extern ESPLogTrace ESP_FS_LOG;

#define ESP_FS_PORT ESP_FS_LOG
#define ESP_FS_LOG_CLEAN() ESP_FS_PORT.clean()
#define ESP_FS_FLUSH() ESP_FS_PORT.flush()
#define ESP_FS_PRINTF(f_, ...)  ESP_FS_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)
#define ESP_FS_PRINTFLF(f_, ...)  ESP_FS_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)
#define ESP_FS_TAG_PRINTF(tag, f_, ...)  ESP_FS_PORT.printf_P(PSTR(tag f_), ##__VA_ARGS__)
#define ESP_FS_FUNCTION_PRINTF(f_, ...)  ESP_FS_PORT.printf_P(PSTR(f_), __FUNCTION__, ##__VA_ARGS__)
#define ESP_FS_FUNCTION_PRINTFLF(f_, ...)  ESP_FS_PORT.printf_P(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)

#define ESP_FS_LOGE(...) do{if(g_fsLevel > 0) {ESP_FS_PRINTFLF("E: " __VA_ARGS__);}}while(0)
#define ESP_FS_LOGW(...) do{if(g_fsLevel > 1) {ESP_FS_PRINTFLF("W: " __VA_ARGS__);}}while(0)
#define ESP_FS_LOGI(...) do{if(g_fsLevel > 2) {ESP_FS_PRINTFLF("I: " __VA_ARGS__);}}while(0)
#define ESP_FS_LOGD(...) do{if(g_fsLevel > 3) {ESP_FS_PRINTFLF("D: " __VA_ARGS__);}}while(0)
#define ESP_FS_LOGV(...) do{if(g_fsLevel > 4) {ESP_FS_PRINTFLF("V: " __VA_ARGS__);}}while(0)

#define ESP_FS_TAG_LOGE(x, ...) do{if(g_fsLevel > 0) {ESP_FS_PRINTFLF("E " x ": " __VA_ARGS__);}}while(0)
#define ESP_FS_TAG_LOGW(x, ...) do{if(g_fsLevel > 1) {ESP_FS_PRINTFLF("W " x ": " __VA_ARGS__);}}while(0)
#define ESP_FS_TAG_LOGI(x, ...) do{if(g_fsLevel > 2) {ESP_FS_PRINTFLF("I " x ": " __VA_ARGS__);}}while(0)
#define ESP_FS_TAG_LOGD(x, ...) do{if(g_fsLevel > 3) {ESP_FS_PRINTFLF("D " x ": " __VA_ARGS__);}}while(0)
#define ESP_FS_TAG_LOGV(x, ...) do{if(g_fsLevel > 4) {ESP_FS_PRINTFLF("V " x ": " __VA_ARGS__);}}while(0)

#if (g_fsLevel > 0)
#define ESP_FS_FUNCTION_IN(...) do{ESP_FS_FUNCTION_PRINTFLF("FUNCTION [%s] IN " __VA_ARGS__);}while(0)
#define ESP_FS_FUNCTION_OUT(...) do{ESP_FS_FUNCTION_PRINTFLF("FUNCTION [%s] OUT " __VA_ARGS__);}while(0)
#define ESP_FS_FUNCTION_LOGI(...) do{ESP_FS_FUNCTION_PRINTFLF("I: FUNCTION [%s] " __VA_ARGS__);}while(0)
#define ESP_FS_FUNCTION_TAG_LOGI(x, ...) do{ESP_FS_FUNCTION_PRINTFLF("I " x ": FUNCTION [%s] " __VA_ARGS__);}while(0)
#else
#define ESP_FS_FUNCTION_IN(...)
#define ESP_FS_FUNCTION_OUT(...)
#define ESP_FS_FUNCTION_LOGI(...)
#define ESP_FS_FUNCTION_TAG_LOGI(x, ...)
#endif

#endif // __ESP_LOG_TRACE_H