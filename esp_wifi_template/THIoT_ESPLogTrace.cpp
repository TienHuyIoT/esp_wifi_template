#include <Ticker.h>
#include <stdarg.h>
#include "esp_task_wdt.h"
#include "THIoT_APPConfig.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_PFTicker.h"
#include "THIoT_ESPMessageQueue.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPMessageList.h"
#include "THIoT_ESPFSHandle.h"
#include "THIoT_ESPTaskMonitor.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPTaskConfig.h"
#include "THIoT_ESPTaskHelper.h"
#include "THIoT_ESPLogTrace.h"

#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#endif

#define LOG_TRACE_TAG_CONSOLE(...)          //SERIAL_TAG_LOGI("[LOG_MODULE]", __VA_ARGS__)
#define LOG_TRACE_TASK_TAG_CONSOLE(...)     //SERIAL_TAG_LOGI("[LOG_MODULE]", __VA_ARGS__)
#define LOG_TRACE_FUNCTION_CONSOLE(...)     //SERIAL_FUNCTION_TAG_LOGI("[LOG_MODULE]", __VA_ARGS__)

#define LOG_TRACE_TAG_LOG(...)              do { if (_fileName != LOG_TRACE_SD_FILE_PATH) {FS_TAG_LOGI("[LOG_MODULE]", __VA_ARGS__);} } while (0)
#define LOG_TRACE_TAG_MUTEX                 "[LOG_TRACE]"

#define LOG_TRACE_WRITE_RETRY_CNT       5U
#define LOG_TRACE_TASK_QUEUE_TIMEOUT_MS 30000U
/* Buffer of one printfTo() record: a pump total log line is ~110 characters. */
#define LOG_TRACE_WRITE_BUFFER_LENGTH   256U

// Helper function to get the path for pump log total
String getPumpLogTotalPath(FS *fs, esp_rtc_time_t *rtc) {
    char dayPath[PUMP_LOG_TOTAL_PATH_LENGTH_MAX];
    snprintf(dayPath, PUMP_LOG_TOTAL_PATH_LENGTH_MAX, "/%04u/%02u/%04u%02u%02u.csv", rtc->year, rtc->mon, rtc->year, rtc->mon, rtc->mday);
    
    char folderPath[PUMP_LOG_FOLDER_PATH_LENGTH];
    snprintf(folderPath, PUMP_LOG_FOLDER_PATH_LENGTH, "/%04u", rtc->year);
    if (!fs->exists(folderPath)) {
        LOG_TRACE_TAG_CONSOLE("mkdir %s", folderPath);
        fs->mkdir(folderPath);
    }

    snprintf(folderPath, PUMP_LOG_FOLDER_PATH_LENGTH, "/%04u/%02u", rtc->year, rtc->mon);
    if (!fs->exists(folderPath)) {
        LOG_TRACE_TAG_CONSOLE("mkdir %s", folderPath);
        fs->mkdir(folderPath);
    }
    return String(dayPath);
}

#define LOG_TRACE_WDT_ENABLE            0

typedef struct {
    void *arg;
    const char *fileName;   /* target file of the record */
    uint8_t *log;
} log_trace_package_t;

/* One queued item is laid out as:
 *   [void* arg][uint32 nameLength (including '\0')][name][payload]
 * The name travels with the record because the log task resolves the target file
 * only when it drains the queue, long after the producer set it. */
static bool _logTraceItemParse(uint8_t *item, size_t itemSize, log_trace_package_t *logTrace, size_t *length)
{
    const size_t header = sizeof(void*) + sizeof(uint32_t);
    if (!item || !logTrace || !length || itemSize < header) {
        return false;
    }

    const uint32_t nameLength = *(uint32_t*)&item[sizeof(void*)];
    if (nameLength == 0 || itemSize < (header + nameLength)) {
        return false;
    }

    logTrace->arg = (void*)(*(int*)&item[0]);
    logTrace->fileName = (const char*)(item + header);
    logTrace->log = item + header + nameLength;
    *length = itemSize - header - nameLength;
    return true;
}

const char log_trace_header[] PROGMEM = "============= MonBox LOG =============";
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
#define LOG_TRACE_BUFFER_SIZE           4096U
static RingbufHandle_t _log_trace_ring_buf_handle;
#else
#define LOG_TRACE_QUEUE_BUFFER_SIZE     64U
ESPMessageQueue _log_trace_queue;
#endif
static SemaphoreHandle_t _log_trace_task_mutex = nullptr;
/* Option (b): "delete done" signal of the deferred delete ticker. */
static SemaphoreHandle_t _log_trace_delete_sem = nullptr;
bool _log_trace_task_running = false;
TaskHandle_t _log_trace_task_handle = nullptr;
#if (TASK_DELETE_USING_TICKER == 1)
Ticker _log_trace_tick;
#endif
#if (TASK_TRACE_LOG_STATIC == 1)
StackType_t  *_log_trace_xStack;
StaticTask_t *_log_trace_xTaskBuffer;
#endif
static bool _start_log_trace_task(void *pvParameters);
static void _task_log_trace_poll(void *pvParameters);

void logTraceTaskBegin() {
    _start_log_trace_task(nullptr);
}

static bool _start_log_trace_task(void *pvParameters) {
    if (!_log_trace_task_mutex) {
        _log_trace_task_mutex = xSemaphoreCreateMutex();
        if (!_log_trace_task_mutex) {
            return false;
        }
        _log_trace_delete_sem = task_delete_sem_create();
    }
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
    if (!_log_trace_ring_buf_handle) {
        _log_trace_ring_buf_handle = xRingbufferCreate(LOG_TRACE_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    }
#else
    if (!_log_trace_queue.isValid()) {
        if (!_log_trace_queue.bufferCreate(LOG_TRACE_QUEUE_BUFFER_SIZE, "log_trace")) {
            /* N14: without the queue every record is dropped and the consumer would spin on a
             * closed queue; the serial channel (always on, no queue involved) is the only one
             * left to report this. */
            SERIAL_TAG_LOGE("[LOG_TRACE]", "log_trace queue create FAILED - task not started");
            return false;
        }
    }
#endif

    if (_log_trace_task_handle == NULL || !_log_trace_task_running) {
        /* Option (b): wait for the "delete done" signal with the mutex released. */
        xSemaphoreTake(_log_trace_task_mutex, portMAX_DELAY);
        if (_log_trace_task_handle != NULL) { // Ensure the task is deleted before creating a new one
            task_wait_pending_delete(_log_trace_task_mutex, _log_trace_delete_sem);
        }
        if (_log_trace_task_handle == NULL) {
            LOG_TRACE_TASK_TAG_CONSOLE("Creating log trace task");
    #if (TASK_TRACE_LOG_STATIC == 1)
            if (!_log_trace_xStack) {
                _log_trace_xStack = (StackType_t *)malloc(TASK_TRACE_LOG_STACK);
            }
            if (!_log_trace_xTaskBuffer) {
                _log_trace_xTaskBuffer = (StaticTask_t *)malloc(sizeof(StaticTask_t));
            }

            _log_trace_task_handle = xTaskCreateStaticPinnedToCore(
                _task_log_trace_poll, TASK_TRACE_LOG_NAME, TASK_TRACE_LOG_STACK
                , NULL // When no parameter is used, simply pass NULL
                , TASK_TRACE_LOG_PRIORITY  // Priority
                , _log_trace_xStack
                , _log_trace_xTaskBuffer
                , TASK_TRACE_LOG_RUNNING_CORE // Core on which the task will run
            );
    #else
            xTaskCreatePinnedToCore(
                _task_log_trace_poll, TASK_TRACE_LOG_NAME, TASK_TRACE_LOG_STACK
                , NULL // When no parameter is used, simply pass NULL
                , TASK_TRACE_LOG_PRIORITY  // Priority
                , &_log_trace_task_handle
                , TASK_TRACE_LOG_RUNNING_CORE // Core on which the task will run
            );
    #endif
            LOG_TRACE_TASK_TAG_CONSOLE("Create Task %lu", _log_trace_task_handle);
        }
        xSemaphoreGive(_log_trace_task_mutex);
    }
    return _log_trace_task_handle != NULL;
}

static void _task_log_trace_poll(void *pvParameters) {
    LOG_TRACE_TASK_TAG_CONSOLE("Log trace task start");
    _log_trace_task_running = true;
    while (_log_trace_task_running) {
        //Receive an item from no-split ring buffer
        size_t item_size;
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
        uint8_t *item = (uint8_t *)xRingbufferReceive(_log_trace_ring_buf_handle, &item_size, portMAX_DELAY);
#else
        /* Fix-review section 4: bounded receive.  A portMAX_DELAY here parks the consumer
         * forever if the producer side dies mid-teardown - and it also keeps the consumer
         * "inside the module" from the queue's point of view, blocking any teardown.  1 s
         * keeps the task responsive (the loop simply re-iterates) at the cost of one extra
         * pass per second when idle. */
        uint8_t *item = (uint8_t *)_log_trace_queue.bufferReceive(&item_size, pdMS_TO_TICKS(1000));
#endif
        if (item) {
            log_trace_package_t logTrace = {};
            size_t length = 0;
            if (!_logTraceItemParse(item, item_size, &logTrace, &length)) {
                LOG_TRACE_TAG_CONSOLE("[Rx]Invalid item, size = %u", item_size);
                length = 0;
            }
            LOG_TRACE_TAG_CONSOLE("xRingbufferReceive");
            LOG_TRACE_TAG_CONSOLE("[Rx]Size = %u", item_size);
            LOG_TRACE_TAG_CONSOLE("[Rx]arg = %u", logTrace.arg);
            LOG_TRACE_TAG_CONSOLE("[Rx]length = %u", length);
            size_t wLength = 0;
            int wCounterLimit = LOG_TRACE_WRITE_RETRY_CNT;
            while (length > 0 && wCounterLimit >= 0) {
#if (LOG_TRACE_WDT_ENABLE == 1)
                if(esp_task_wdt_add(NULL) != ESP_OK){
                    log_e("Failed to add async task to WDT");
                }
#endif

                size_t wLog = ESPLogTrace::_s_eventDataHandleTo(logTrace.arg, logTrace.fileName, logTrace.log + wLength, length);
                length -= wLog;
                wLength += wLog;

#if (LOG_TRACE_WDT_ENABLE == 1)
                if(esp_task_wdt_delete(NULL) != ESP_OK){
                    log_e("Failed to remove loop task from WDT");
                }
#endif
                if (0 == length) {
                    LOG_TRACE_TAG_CONSOLE("Write log succeed");
                } else {
                    /** 
                     * User case: File can't open or file write fail.
                    */
                    --wCounterLimit;
                    LOG_TRACE_TAG_CONSOLE("Write log failure %u", retry);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
            }

            //Return Item
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
            vRingbufferReturnItem(_log_trace_ring_buf_handle, (void *)item);
#else
            _log_trace_queue.bufferReturnItem((void *)item);
#endif
        }
    }

    _log_trace_task_running = false;
    LOG_TRACE_TASK_TAG_CONSOLE("task stop");
    LOG_TRACE_TASK_TAG_CONSOLE("Waiting for delete task");
    if (!task_alloc_delete_ticker(TASK_DELETE_DELAY_MS, [](void *arg)
    {
        xSemaphoreTake(_log_trace_task_mutex, portMAX_DELAY);
        if (_log_trace_task_handle != nullptr) {
            TaskHandle_t tempHandle = _log_trace_task_handle;
            /* Pattern B - external delete: vTaskDelete() first, so the TCB is
             * never linked into the idle cleanup list; only then free. */
            vTaskDelete(_log_trace_task_handle);
            _log_trace_task_handle = nullptr;
            if (_log_trace_xStack) {
                free(_log_trace_xStack);
                _log_trace_xStack = nullptr;
            }
            if (_log_trace_xTaskBuffer) {
                free(_log_trace_xTaskBuffer);
                _log_trace_xTaskBuffer = nullptr;
            }
            LOG_TRACE_TASK_TAG_CONSOLE("log trace task cleanup complete for task handle %u", tempHandle);
        }
        xSemaphoreGive(_log_trace_task_mutex);
        /* Option (b): announce "delete done" to the recreate path. */
        task_delete_sem_signal(_log_trace_delete_sem);
    })) {
        LOG_TRACE_TASK_TAG_CONSOLE("delete ticker alloc failed: clear handle inline");
        task_delete_fallback(_log_trace_task_mutex, &_log_trace_task_handle);
        vTaskDelete(NULL); /* no ticker: delete ourselves */
    }

    /* Pattern B: parked until the delete ticker removes us. */
    vTaskDelay(portMAX_DELAY);
}

ESPLogTrace::ESPLogTrace(FS &fs,
                         const char *fileName,
                         size_t limitSize,
                         const char *header,
                         bool timeLogOn,
                         bool runtimeOn,
                         bool taskHandle,
                         TickType_t timeoutQueueRx,
                         TickType_t timeoutQueueTx,
                         TickType_t timeoutMutexFile,
                         int queueSize)
    : _fileName(fileName), _header(header), _limitSize(limitSize),
      _timeLogOn(timeLogOn), _runtimeOn(runtimeOn),
      _stateMutex(xSemaphoreCreateRecursiveMutex()),
      _task_mutex(xSemaphoreCreateMutex()),
      _task_delete_sem(task_delete_sem_create())
{
    _queueSize = queueSize;
    _timeoutQueueRx = timeoutQueueRx;
    _timeoutQueueTx = timeoutQueueTx;
    _timeoutMutexFile = timeoutMutexFile;
    _enable = true;
    _fileSize = 0;
    _fileSizeName = fileName;
    _fs = &fs;
    _task_queue = nullptr;
    _task_handle = nullptr;
    _xStack = nullptr;
    _xTaskBuffer = nullptr;
    _task_running = false;
    _taskHandleEnabled = taskHandle;
    _queueName = _fileName;
}

ESPLogTrace::~ESPLogTrace() {
    if (_task_running) {
        _task_running = false;
        delay(100); // Ensure the task is deleted before destroying the queue
        xSemaphoreTake(_task_mutex, portMAX_DELAY);
        if (_task_handle) {
            vTaskDelete(_task_handle);
            _task_handle = nullptr;
        }
        if (_xStack) {
            free(_xStack);
            _xStack = nullptr;
        }
        if (_xTaskBuffer) {
            free(_xTaskBuffer);
            _xTaskBuffer = nullptr;
        }
        xSemaphoreGive(_task_mutex);
    }

    if (_task_queue) {
        _task_queue->end();
        _task_queue = nullptr;
    }

    if (_task_mutex) {
        vSemaphoreDelete(_task_mutex);
        _task_mutex = nullptr;
    }

    if (_stateMutex) {
        vSemaphoreDelete(_stateMutex);
        _stateMutex = nullptr;
    }
}

bool ESPLogTrace::_stateLock() {
    /* Created on first use (the instances are global objects, so a mutex created in
     * the constructor would run before the scheduler). */
    if (!_stateMutex) {
        _stateMutex = xSemaphoreCreateRecursiveMutex();
        if (!_stateMutex) {
            return false;
        }
    }
    return xSemaphoreTakeRecursive(_stateMutex, portMAX_DELAY) == pdTRUE;
}

void ESPLogTrace::_stateUnlock() {
    if (_stateMutex) {
        xSemaphoreGiveRecursive(_stateMutex);
    }
}

void ESPLogTrace::enterCritical() {
    _stateLock();
}

void ESPLogTrace::exitCritical() {
    _stateUnlock();
}

void ESPLogTrace::_storeProgress(const char *fileName, size_t fileSize) {
    if (!_stateLock()) {
        return;
    }
    _fileSize = fileSize;
    _fileSizeName = fileName;
    _stateUnlock();
}

void ESPLogTrace::fsHandleSet(FS &fs) { 
    _stateLock();
    _fs = &fs;
    if (_fs == &NAND_FS_SYSTEM) {
        _fsType = FS_NAND_HANDLER_TYPE;
    } else {
        _fsType = SD_CARD_HANDLER_TYPE;
    }
    _stateUnlock();
}

void ESPLogTrace::fileNameSet(const char *fileName) {
    if (!fileName) {
        return;
    }
    _stateLock();
    _fileName = fileName;
    _stateUnlock();
}

String ESPLogTrace::fileNameGet() {
    if (!_stateLock()) {
        return String();
    }
    const String fileName = _fileName;
    _stateUnlock();
    return fileName;
}

void ESPLogTrace::limitSizeSet(size_t limitSize) {
    _stateLock();
    _limitSize = limitSize;
    _stateUnlock();
}

void ESPLogTrace::headerFile(const char *header) {
    if (!header) {
        return;
    }
    _stateLock();
    _header = header;
    _stateUnlock();
}

void ESPLogTrace::enableSet(bool enable) {
    _stateLock();
    _enable = enable;
    _stateUnlock();
}

void ESPLogTrace::enable() {
    _stateLock();
    _enable = true;
    _stateUnlock();
}

void ESPLogTrace::disable() {
    _stateLock();
    _enable = false;
    _stateUnlock();
}

void ESPLogTrace::clean() {
    if (!_stateLock()) {
        return;
    }
    const String fileName = _fileName;
    const String backup = backupName();
    FS *const fs = _fs;
    _stateUnlock();

    if (fileName.length() == 0 || !fs) {
        return;
    }

    mutex_status_t lockReason = MUTEX_STATUS_OK;
    if (mutexFileList.takeEx(fileName.c_str(), pdMS_TO_TICKS(_timeoutMutexFile),
                             LOG_TRACE_TAG_MUTEX, &lockReason)) {
        fs->remove(backup);
        fs->remove(fileName);
        mutexFileList.give(fileName.c_str(), LOG_TRACE_TAG_MUTEX);
    } else {
        LOG_TRACE_TAG_CONSOLE("Mutex list take FAIL %s (%s)", fileName.c_str(), mutexStatusToString(lockReason));
    }
}

size_t ESPLogTrace::_s_eventDataHandle(void *arg, const uint8_t *log, size_t length)  {
    ESPLogTrace* handle = reinterpret_cast<ESPLogTrace*>(arg);
    return handle->_addLog(log, length);
}

size_t ESPLogTrace::_s_eventDataHandleTo(void *arg, const char *fileName, const uint8_t *log, size_t length)  {
    ESPLogTrace* handle = reinterpret_cast<ESPLogTrace*>(arg);
    return handle->_addLogTo(fileName, log, length);
}

size_t ESPLogTrace::_addLog(const uint8_t *buffer, size_t length) {
    /* A record written through write(): use the file the object points at now. */
    if (!_stateLock()) {
        return 0;
    }
    const String fileName = _fileName;
    _stateUnlock();

    if (fileName.length() == 0) {
        return 0;
    }
    return _addLogTo(fileName.c_str(), buffer, length);
}

size_t ESPLogTrace::_addLogTo(const char *fileName, const uint8_t *buffer, size_t length) {
    if (!fileName || fileName[0] == '\0' || !buffer || length == 0) {
        return 0;
    }

    /* Snapshot the shared state under the lock, then release it before waiting for
     * the file lock: holding it across that wait would invert the lock order against
     * the FS editor (file lock -> log) and stall every writer of this object. */
    if (!_stateLock()) {
        return 0;
    }
    const bool enabled = _enable;
    if (_fs == &SD_FS_SYSTEM && !SDCard.statusIsOk()) {
        _fsType = FS_NAND_HANDLER_TYPE;
        _fs = &NAND_FS_SYSTEM;
        _limitSize = LOG_INTERNAL_MEMORY_SIZE_LIMITED;
        LOG_TRACE_TAG_CONSOLE("%s Auto change fs handle to internal memory", fileName);
    }
    FS *const fs = _fs;
    const size_t limitSize = _limitSize;
    const bool sizeKnown = (_fileSizeName == fileName);
    const size_t fileSize = sizeKnown ? _fileSize : 0;
    _stateUnlock();

    if (!enabled) {
        /* Kept from the legacy code: a disabled trace discards the record. */
        return length;
    }

    mutex_status_t lockReason = MUTEX_STATUS_OK;
    bool isLock = mutexFileList.takeEx(fileName, pdMS_TO_TICKS(_timeoutMutexFile),
                                       LOG_TRACE_TAG_MUTEX, &lockReason); // locked file
    if (!isLock) {
        /* ML5: the reason ("timeout", "list-full", "admission-limit", ...) is now reported
         * instead of a single bare false that the caller could only drop. */
        LOG_TRACE_TAG_CONSOLE("Mutex list take FAIL %s (%s)", fileName, mutexStatusToString(lockReason));
        return 0;
    }
    bool isNew = assertOverSize(fs, fileName, fileSize);
    if (isNew) {
        LOG_TRACE_TAG_CONSOLE("%s file size is over limit, backup to folder", fileName);
    }
    size_t newSize = isNew ? 0 : fileSize;

    File fs_handle;
    if (!fs->exists(fileName)) {
        /* F14: create the target on the FIRST attempt. The legacy "try again"
         * debounce dropped the first 1-2 records of every newly appearing file, and
         * the counter it used was shared by every target path of this object. */
        LOG_TRACE_TAG_CONSOLE("%s not exists, create new file", fileName);
        fs_handle = fs->open(fileName, "w");
        if (!fs_handle) {
            mutexFileList.give(fileName, LOG_TRACE_TAG_MUTEX);
            _storeProgress(fileName, newSize);
            return 0;
        }
        newSize = 0;
        if (_header.length() > 0) {
            newSize = fs_handle.println(_header);
        }
    } else {
        fs_handle = fs->open(fileName, "a");
        if (!fs_handle) {
            mutexFileList.give(fileName, LOG_TRACE_TAG_MUTEX);
            _storeProgress(fileName, newSize);
            return 0;
        }
        newSize = fs_handle.size();
    }

    LOG_TRACE_TAG_CONSOLE("%s size before %u", fileName, newSize);
    size_t w = fs_handle.write(buffer, length);
    newSize += w;
    fs_handle.close();
    LOG_TRACE_TAG_CONSOLE("%s size after %u, w = %u", fileName, newSize, w);
    mutexFileList.give(fileName, LOG_TRACE_TAG_MUTEX);  // unlock file
    _storeProgress(fileName, newSize);
    return w;
}

bool ESPLogTrace::assertOverSize(FS *fs, const char *fileName, size_t fileSize) {
    bool status = false;
    if (!fs || !fileName) {
        return false;
    }
    if (!_stateLock()) {
        return false;
    }
    const size_t limitSize = _limitSize;
    _stateUnlock();

    if (fileSize >= limitSize) {
#if (LOG_TRACE_WDT_ENABLE == 1)
        esp_task_wdt_reset();
#endif
        LOG_TRACE_TAG_CONSOLE("File size = %lu, limit size = %lu", fileSize, limitSize);
        status = FSHandle.backup2Folder(*fs, fileName);
#if (LOG_TRACE_WDT_ENABLE == 1)
        esp_task_wdt_reset();
#endif
    }
    return status;
}

size_t ESPLogTrace::writeTo(const char *fileName, const uint8_t *buffer, size_t size) {
    if (!fileName || fileName[0] == '\0' || !buffer || size == 0) {
        return 0;
    }
    if (!_stateLock()) {
        return 0;
    }
    const bool enabled = _enable;
    _stateUnlock();
    if (!enabled) {
        return 0;
    }
    return _enqueue(fileName, buffer, size);
}

size_t ESPLogTrace::printfTo(const char *fileName, const char *format, ...) {
    if (!fileName || !format) {
        return 0;
    }

    /* Same shape as Print::printf(): format into a stack buffer and fall back to the
     * heap when the record does not fit, so a long record is never truncated.
     * va_copy() keeps the second pass over the arguments valid. */
    char loc_buf[LOG_TRACE_WRITE_BUFFER_LENGTH];
    char *temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
    va_end(copy);
    if (len < 0) {
        va_end(arg);
        return 0;
    }
    if (len >= (int)sizeof(loc_buf)) {
        temp = (char*)malloc(len + 1);
        if (temp == NULL) {
            va_end(arg);
            return 0;
        }
        len = vsnprintf(temp, len + 1, format, arg);
    }
    va_end(arg);

    const size_t written = writeTo(fileName, reinterpret_cast<const uint8_t *>(temp), (size_t)len);
    if (temp != loc_buf) {
        free(temp);
    }
    return written;
}

// Should be have return size value to prevent crash.
size_t ESPLogTrace::write(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    /* Snapshot the target file of this record, so the log task writes it to the file
     * that was current when the record was created, even if _fileName changes while
     * the item waits in the queue. */
    if (!_stateLock()) {
        return 0;
    }
    const bool enabled = _enable;
    const String fileName = _fileName;
    _stateUnlock();

    if (!enabled || fileName.length() == 0) {
        return 0;
    }
    return _enqueue(fileName.c_str(), buffer, size);
}

/* One queued item is: [void* arg][uint32 nameLength (including '\0')][name][payload].
 * The file name travels with the record because every instance writes through the
 * shared log task, which resolves the target file only when it drains the queue. */
size_t ESPLogTrace::_enqueue(const char *fileName, const uint8_t *buffer, size_t size) {
    if (!fileName || fileName[0] == '\0' || !buffer || size == 0) {
        return 0;
    }

    if (_taskHandleEnabled) {
        if (!_queueInit()) {
            return 0;
        }
    }

    const size_t nameLength = strlen(fileName) + 1; /* including '\0' */

    constexpr int TIME_BUFF_SIZE = 60;
    char timeBuff[TIME_BUFF_SIZE];
    char *pBuff = timeBuff;
    size_t buffSize = TIME_BUFF_SIZE;
    size_t timeLength = 0;

    if (_timeLogOn && buffer[0] != 0xFF) {
        String time = ESPTime.toStringLog();
        time.toCharArray(pBuff, buffSize);
        size_t length = strlen(pBuff);
        buffSize -= length;
        pBuff += length;
        timeLength += length;
    }

    if (_runtimeOn && buffer[0] != 0xFF) {
        size_t runTime = millis();
        size_t days = runTime / (1000u * 86400u);
        size_t mSec = runTime % (1000u * 86400u);

        size_t messages = 0;
        if (_taskHandleEnabled) {
            messages = _task_queue->messageAvailable();
        } else {
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 1)
            messages = _log_trace_queue.messageAvailable();
#endif
        }
        timeLength += snprintf(pBuff, buffSize, "%03u.%08u_%06u_%02u, ", days, mSec, ESP.getFreeHeap(), messages + 1);
    }

    size_t buffLength = size;
    // If the first byte is 0xFF, it means that the time is not needed
    // and the runtime is not needed, so we skip it.
    if (buffer[0] == 0xFF && (_timeLogOn || _runtimeOn)) {
        buffer++;
        size--;
        if (size == 0) {
            LOG_TRACE_TAG_CONSOLE("Nothing to write");
            return 0;
        }
    }

    const size_t payloadOffset = sizeof(void*) + sizeof(uint32_t) + nameLength;
    size_t writeLength = payloadOffset + timeLength + size;

    //Send an item
    uint8_t *item = (uint8_t*)malloc(writeLength);
    if (!item) {
        LOG_TRACE_TAG_CONSOLE("Failed to allocate item");
        return 0;
    }
    
    *(int*)&item[0] = (int)this;
    *(uint32_t*)&item[sizeof(void*)] = (uint32_t)nameLength;
    memcpy(item + sizeof(void*) + sizeof(uint32_t), fileName, nameLength);
    if (timeLength > 0) {
        memcpy(item + payloadOffset, timeBuff, timeLength);
    }
    memcpy(item + payloadOffset + timeLength, buffer, size);
    LOG_TRACE_TAG_CONSOLE("xRingbufferSend");
    LOG_TRACE_TAG_CONSOLE("[Tx]Size = %u", writeLength);
    LOG_TRACE_TAG_CONSOLE("[Tx]arg = %u(this), %u(arg)", this, *(int*)&item[0]);
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
    UBaseType_t res = xRingbufferSend(_log_trace_ring_buf_handle, item, writeLength, portMAX_DELAY);
#else
    UBaseType_t res;
    if (_taskHandleEnabled) {
        res = _task_queue->bufferSend(item, writeLength, pdMS_TO_TICKS(_timeoutQueueTx));
        _startTask();
    } else {
        res = _log_trace_queue.bufferSend(item, writeLength, pdMS_TO_TICKS(_timeoutQueueTx));
    }
#endif

    free(item); // Free the item after sending it to avoid memory leak

    size_t queueMessageAvailable = 0;
    uint32_t liveTimeQueueMessage = 0;
    if (_taskHandleEnabled) {
        queueMessageAvailable = _task_queue->messageAvailable();
        liveTimeQueueMessage = _task_queue->liveTime();
    } else {
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 1)
        queueMessageAvailable = _log_trace_queue.messageAvailable();
        liveTimeQueueMessage = _log_trace_queue.liveTime();
#endif
    }

    if (res != pdTRUE) {
        LOG_TRACE_TAG_LOG("File %s failed to send item, live time %u, messages %u", fileName, liveTimeQueueMessage, queueMessageAvailable);
        SOFTReset.enable(500, ESPSoftReset::LOG_TRACE_QUEUE_TYPE);
        return 0;
    }

    /* Q4 / MQ22: residency alone is not proof of a stall - a slow file lock can push one
     * record past the threshold while the consumer keeps draining.  Require that the
     * consumer made no progress during the window (drained() counter of the queue). */
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 1)
    static size_t s_traceDrainedLast = 0;
    static uint32_t s_traceProgressMs = 0;
    const size_t traceDrained = _log_trace_queue.drained();
    const uint32_t traceNowMs = millis();
    if (traceDrained != s_traceDrainedLast) {
        s_traceDrainedLast = traceDrained;
        s_traceProgressMs = traceNowMs;
    }
    const bool consumerStalled = ((traceNowMs - s_traceProgressMs) > LOG_TRACE_TASK_QUEUE_TIMEOUT_MS);
#else
    const bool consumerStalled = true;   /* ring path has no progress metric */
#endif

    if ((liveTimeQueueMessage > LOG_TRACE_TASK_QUEUE_TIMEOUT_MS) && consumerStalled) {
        // do reset
        LOG_TRACE_TAG_LOG("Queue message %s live timeout %u (no drain), messages %u", fileName, liveTimeQueueMessage, queueMessageAvailable);
        SOFTReset.enable(500, ESPSoftReset::LOG_TRACE_QUEUE_TYPE);
        return 0;
    }
    return buffLength;
}

void ESPLogTrace::flush() {
    uint8_t timeoutCounter = 0;
    while (!isFinished() && timeoutCounter < 20) {
        esp_task_wdt_reset(); /* Avoid reset by wdt of the task */
        vTaskDelay(pdMS_TO_TICKS(100));
        timeoutCounter++;
    }
    if (timeoutCounter >= 20) {
        LOG_TRACE_TAG_CONSOLE("Flush timeout, queue is not empty");
    } else {
        LOG_TRACE_TAG_CONSOLE("Flush completed, queue is empty");
    }
}

bool ESPLogTrace::isFinished() {
    /* Watch the queue this object actually writes through: an instance owning its own
     * task queue must not report the state of the shared log task queue. */
    if (_taskHandleEnabled) {
        return (_task_queue == nullptr) || _task_queue->isEmpty();
    }
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 1)
    return _log_trace_queue.isEmpty();
#else
    return true; /* ring buffer build: there is no shared message queue to watch */
#endif
}

size_t ESPLogTrace::size() {
    if (!_stateLock()) {
        return 0;
    }
    const String fileName = _fileName;
    FS *const fs = _fs;
    _stateUnlock();

    if (fileName.length() == 0 || !fs) {
        return 0;
    }

    size_t length = 0;
    /* Same file lock as _addLogTo(): never read the file while the log task writes
     * it. The state mutex is not held here (lock order: state -> file). */
    mutex_status_t lockReason = MUTEX_STATUS_OK;
    if (!mutexFileList.takeEx(fileName.c_str(), pdMS_TO_TICKS(_timeoutMutexFile),
                              LOG_TRACE_TAG_MUTEX, &lockReason)) {
        LOG_TRACE_TAG_CONSOLE("Mutex list take FAIL %s (%s)", fileName.c_str(), mutexStatusToString(lockReason));
        return 0;
    }

    File fs_handle = fs->open(fileName, "r");
    if (!fs_handle) {
        LOG_TRACE_TAG_CONSOLE("Open log file %s Error", fileName.c_str());
    } else {
        length = fs_handle.size();
        fs_handle.close();
    }
    mutexFileList.give(fileName.c_str(), LOG_TRACE_TAG_MUTEX);

    if (length == 0) {
        LOG_TRACE_TAG_CONSOLE("File %s is empty", fileName.c_str());
    }
    return length;
}

bool ESPLogTrace::_queueInit() {
    if (_task_queue) {
        return true;
    }

    /* write() and _startTask() may reach here at the same time, so the queue is created
     * once, under the task mutex. */
    if (!_task_mutex) {
        _task_mutex = xSemaphoreCreateMutex();
        if (!_task_mutex) {
            return false;
        }
    }

    xSemaphoreTake(_task_mutex, portMAX_DELAY);
    if (!_task_queue) {
        ESPMessageQueue *queue = new (std::nothrow) ESPMessageQueue();
        if (queue) {
            queue->bufferCreate(_queueSize, _queueName.c_str());
            if (queue->isValid()) {
                _task_queue = queue;
                LOG_TRACE_TASK_TAG_CONSOLE("Task queue created for %s", _queueName.c_str());
            } else {
                delete queue;
            }
        }
    }
    xSemaphoreGive(_task_mutex);
    return _task_queue != nullptr;
}

bool ESPLogTrace::_startTask() {
    if (!_task_mutex) {
        _task_mutex = xSemaphoreCreateMutex();
        if (!_task_mutex) {
            return false;
        }
    }
    if (!_queueInit()) {
        LOG_TRACE_TASK_TAG_CONSOLE("Failed to initialize task queue for %s", _fileName.c_str());
        return false;
    }
    if (_task_handle == NULL || !_task_running) {
        /* Option (b): wait for the "delete done" signal with the mutex released. */
        xSemaphoreTake(_task_mutex, portMAX_DELAY);
        if (_task_handle != NULL) { // Ensure the task is deleted before creating a new one
            task_wait_pending_delete(_task_mutex, _task_delete_sem);
        }
        if (_task_handle == NULL)
        {
            if (!_xStack) {
                _xStack = (StackType_t *)malloc(TASK_TRACE_LOG_STACK);
            }
            if (!_xTaskBuffer) {
                _xTaskBuffer = (StaticTask_t *)malloc(sizeof(StaticTask_t));
            }
            _task_handle = xTaskCreateStaticPinnedToCore(_task,
                                    "LogTraceTask",
                                    TASK_TRACE_LOG_STACK,
                                    this,
                                    TASK_TRACE_LOG_PRIORITY,
                                    _xStack,
                                    _xTaskBuffer,
                                    TASK_TRACE_LOG_RUNNING_CORE);

            if (_task_handle != nullptr) {
                LOG_TRACE_TASK_TAG_CONSOLE("Log trace task handle created for %s: %u", _fileName.c_str(), _task_handle);
            }
        }
        xSemaphoreGive(_task_mutex);
    }
    return _task_handle != NULL;
}

void ESPLogTrace::_task(void *pvParameters) {
    ESPLogTrace *self = static_cast<ESPLogTrace *>(pvParameters);
    self->_task_running = true;
    LOG_TRACE_TASK_TAG_CONSOLE("task start");
    self->_loop();
    self->_task_running = false;
    LOG_TRACE_TASK_TAG_CONSOLE("task stop");
    LOG_TRACE_TASK_TAG_CONSOLE("Waiting for delete task");
    if (!task_alloc_delete_ticker(TASK_DELETE_DELAY_MS, [](void *arg) {
        ESPLogTrace *self = static_cast<ESPLogTrace *>(arg);
        xSemaphoreTake(self->_task_mutex, portMAX_DELAY);
        if (self->_task_handle != nullptr) {
            TaskHandle_t taskHandle = self->_task_handle;
            /* Pattern B - external delete: vTaskDelete() first, so the TCB is
             * never linked into the idle cleanup list; only then free. */
            vTaskDelete(self->_task_handle);
            self->_task_handle = nullptr;
            if (self->_task_queue) {
                self->_task_queue->end();
                self->_task_queue = nullptr;
            }
            if (self->_xStack) {
                free(self->_xStack);
                self->_xStack = nullptr;
            }
            if (self->_xTaskBuffer) {
                free(self->_xTaskBuffer);
                self->_xTaskBuffer = nullptr;
            }
            LOG_TRACE_TASK_TAG_CONSOLE("task handle deleted for %s: %u", self->_fileName.c_str(), taskHandle);
        }
        xSemaphoreGive(self->_task_mutex);
        /* Option (b): announce "delete done" to the recreate path. */
        task_delete_sem_signal(self->_task_delete_sem);
    }, self)) {
        LOG_TRACE_TASK_TAG_CONSOLE("delete ticker alloc failed: clear handle inline for %s", self->_fileName.c_str());
        task_delete_fallback(self->_task_mutex, &self->_task_handle);
        vTaskDelete(NULL); /* no ticker: delete ourselves */
    }

    /* Pattern B: parked until the delete ticker removes us. */
    vTaskDelay(portMAX_DELAY);
}

void ESPLogTrace::_loop() {
    while (_task_running) {
        size_t item_size;
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
        uint8_t *item = (uint8_t *)xRingbufferReceive(_log_trace_ring_buf_handle, &item_size, portMAX_DELAY);
#else
        uint8_t *item = (uint8_t *)_task_queue->bufferReceive(&item_size, pdMS_TO_TICKS(_timeoutQueueRx));
        if (!item) {
            LOG_TRACE_TAG_CONSOLE("No item received, timeout or empty queue");
            break;
        }
#endif
        log_trace_package_t logTrace = {};
        size_t length = 0;
        if (!_logTraceItemParse(item, item_size, &logTrace, &length)) {
            LOG_TRACE_TAG_CONSOLE("[Rx]Invalid item, size = %u", item_size);
            length = 0;
        }
        LOG_TRACE_TAG_CONSOLE("xRingbufferReceive");
        LOG_TRACE_TAG_CONSOLE("[Rx]Size = %u", item_size);
        LOG_TRACE_TAG_CONSOLE("[Rx]arg = %u", logTrace.arg);
        LOG_TRACE_TAG_CONSOLE("[Rx]length = %u", length);
        size_t wLength = 0;
        int wCounterLimit = LOG_TRACE_WRITE_RETRY_CNT;
        while (length > 0 && wCounterLimit >= 0) {
#if (LOG_TRACE_WDT_ENABLE == 1)
            if(esp_task_wdt_add(NULL) != ESP_OK){
                log_e("Failed to add async task to WDT");
            }
#endif

            size_t wLog = _s_eventDataHandleTo(logTrace.arg, logTrace.fileName, logTrace.log + wLength, length);
            length -= wLog;
            wLength += wLog;

#if (LOG_TRACE_WDT_ENABLE == 1)
            if(esp_task_wdt_delete(NULL) != ESP_OK){
                log_e("Failed to remove loop task from WDT");
            }
#endif
            if (0 == length) {
                LOG_TRACE_TAG_CONSOLE("Write log succeed");
            } else {
                /** 
                 * User case: File can't open or file write fail.
                */
                --wCounterLimit;
                LOG_TRACE_TAG_CONSOLE("Write log failure %u", retry);
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }

        //Return Item
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
        vRingbufferReturnItem(_log_trace_ring_buf_handle, (void *)item);
#else
        _task_queue->bufferReturnItem((void *)item);
#endif
    }
}

ESPLogTrace SD_FS_LOG(
    SD_FS_SYSTEM,
    LOG_TRACE_SD_FILE_PATH,
    LOG_TRACE_SD_SIZE_LIMITED,
    log_trace_header,
    true,  // Enable time log
    true,  // Enable runtime log
    false, // Static task handle
    LOG_TRACE_RECEIVE_QUEUE_TIMEOUT_MS,
    LOG_TRACE_SEND_QUEUE_WAIT_MIN,
    LOG_TRACE_FILE_MUTEX_WAIT_MIN,
    LOG_TRACE_QUEUE_TASK_SIZE);

ESPLogTrace ESP_FS_LOG(
    NAND_FS_SYSTEM,
    LOG_TRACE_ESP_FILE_PATH,
    LOG_INTERNAL_MEMORY_SIZE_LIMITED,
    log_trace_header,
    true,  // Enable time log
    true,  // Enable runtime log
    false, // Static task handle
    LOG_TRACE_RECEIVE_QUEUE_TIMEOUT_MS,
    LOG_TRACE_SEND_QUEUE_WAIT_MIN,
    LOG_TRACE_FILE_MUTEX_WAIT_MIN,
    LOG_TRACE_QUEUE_TASK_SIZE);