#include <Ticker.h>
#include "esp_task_wdt.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_APPConfig.h"
#include "THIoT_PFTicker.h"
#include "THIoT_ESPMessageQueue.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPFSHandle.h"
#include "THIoT_ESPTaskConfig.h"
#include "THIoT_ESPTaskHelper.h"
#include "THIoT_PFConsoleTrace.h"
#include "THIoT_PFSerialTrace.h"

#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#endif

/* Definition console macro */
#define CONSOLE_TRACE_PORT                          Serial
#define CONSOLE_TRACE_PRINTFLN(tag, f_, ...)        CONSOLE_TRACE_PORT.printf_P(PSTR(tag f_ "\r\n"), ##__VA_ARGS__)
#define CONSOLE_TRACE_FUNCTION_PRINTFLF(f_, ...)    CONSOLE_TRACE_PORT.printf_P(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)
#define CONSOLE_TRACE_TASK_TAG_I(...)               SERIAL_CONSOLE_I("[CONSOLE TASK]: ", __VA_ARGS__)
#define CONSOLE_TRACE_TASK_TAG_E(...)               SERIAL_CONSOLE_E("[CONSOLE TASK]: ", __VA_ARGS__)

/* Private macro debug */
#define CONSOLE_TRACE_TAG_DEBUG_ENABLE              0
#if (CONSOLE_TRACE_TAG_DEBUG_ENABLE == 1)
#define CONSOLE_TRACE_TAG_DEBUG(...)                CONSOLE_TRACE_PRINTFLN("I CONSOLE TRACE: ", __VA_ARGS__)
#define CONSOLE_TRACE_FUNCTION_TAG_DEBUG(...)       CONSOLE_TRACE_FUNCTION_PRINTFLF("I CONSOLE TRACE: FUNCTION [%s] ", __VA_ARGS__)
#else
#define CONSOLE_TRACE_TAG_DEBUG(...)
#define CONSOLE_TRACE_FUNCTION_TAG_DEBUG(...)
#endif

#define CONSOLE_TRACE_TAG_UDP_DEBUG_ENABLE          1
#define CONSOLE_UDP_DEBUG_PORT                      CONSOLE_TRACE_PORT
#if (CONSOLE_TRACE_TAG_UDP_DEBUG_ENABLE == 1)
#define CONSOLE_TRACE_TAG_UDP_DEBUG(...)            CONSOLE_TRACE_PRINTFLN("I CONSOLE TRACE UDP: ", __VA_ARGS__)
#define CONSOLE_TRACE_UDP_FUNCTION_TAG_DEBUG(...)   CONSOLE_TRACE_FUNCTION_PRINTFLF("I CONSOLE TRACE UDP: FUNCTION [%s] ", __VA_ARGS__)
#else
#define CONSOLE_TRACE_TAG_UDP_DEBUG(...)
#define CONSOLE_TRACE_FUNCTION_TAG_UDP_DEBUG(...)
#endif

#define CONSOLE_TRACE_TAG_FILE_DEBUG_ENABLE          1
#define CONSOLE_FILE_DEBUG_PORT                      CONSOLE_TRACE_PORT
#if (CONSOLE_TRACE_TAG_FILE_DEBUG_ENABLE == 1)
#define CONSOLE_TRACE_TAG_FILE_DEBUG(...)            CONSOLE_TRACE_PRINTFLN("I CONSOLE TRACE FILE: ", __VA_ARGS__)
#define CONSOLE_TRACE_FILE_FUNCTION_TAG_DEBUG(...)   CONSOLE_TRACE_FUNCTION_PRINTFLF("I CONSOLE TRACE FILE: FUNCTION [%s] ", __VA_ARGS__)
#else
#define CONSOLE_TRACE_TAG_FILE_DEBUG(...)
#define CONSOLE_TRACE_FUNCTION_TAG_FILE_DEBUG(...)
#endif

#define CONSOLE_TRACE_WDT_ENABLE            0
#define CONSOLE_TRACE_WRITE_RETRY_CNT       2U
/* Bounded wait for a WebSocket console callback that is still running (see _waitWsIdle). */
#define CONSOLE_TRACE_WS_IDLE_WAIT_MS       500U


#if (gConsoleDebugLevel > 0)
typedef struct {
    void *arg;
    uint8_t *log;
} console_trace_package_t;

#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
#define CONSOLE_TRACE_BUFFER_SIZE           4096U
static RingbufHandle_t _console_trace_ring_buf_handle;
#else
#define CONSOLE_TRACE_QUEUE_BUFFER_SIZE     64U
#define CONSOLE_TRACE_QUEUE_TIMEOUT_MS      100U
ESPMessageQueue _console_trace_queue;
#endif
bool _console_trace_task_running = false;
TaskHandle_t _console_trace_task_handle = nullptr;
/* Option (b): "delete done" signal of the deferred delete ticker. */
static SemaphoreHandle_t _console_trace_delete_sem = nullptr;
#if (TASK_DELETE_USING_TICKER == 1)
Ticker _console_trace_tick;
#endif
#if (TASK_CONSOLE_TRACE_STATIC == 1)
StackType_t  *_console_trace_xStack;
StaticTask_t *_console_trace_xTaskBuffer;
#endif
static bool _start_console_trace_task();
static void _console_trace_task(void *pvParameters);

void consoleTraceTaskBegin() {
    _start_console_trace_task();
}

void consoleTraceTaskEnd() {
    if (_console_trace_task_handle) {
        CONSOLE_TRACE_STREAM.flush();
        _console_trace_task_running = false; // Give a signal to stop the task and delete it
    }
}

static bool _start_console_trace_task() {
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
    if (!_console_trace_ring_buf_handle) {
        _console_trace_ring_buf_handle = xRingbufferCreate(CONSOLE_TRACE_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    }
#else
    if (!_console_trace_queue.isValid()) {
        CONSOLE_TRACE_TASK_TAG_I("Create console trace queue");
        if (!_console_trace_queue.bufferCreate(CONSOLE_TRACE_QUEUE_BUFFER_SIZE, "console_trace")) {
            CONSOLE_TRACE_TASK_TAG_E("Failed to create console trace queue");
            return false;
        }
    }
#endif

    if (!_console_trace_task_handle) {
        _console_trace_task_running = true;
        CONSOLE_TRACE_TASK_TAG_I("Starting console trace task");
        if (!_console_trace_delete_sem) {
            _console_trace_delete_sem = task_delete_sem_create();
        }
#if (TASK_CONSOLE_TRACE_STATIC == 1)
        if (!_console_trace_xStack) {
            _console_trace_xStack = (StackType_t *)malloc(TASK_CONSOLE_TRACE_STACK);
        }
        if (!_console_trace_xTaskBuffer) {
            _console_trace_xTaskBuffer = (StaticTask_t *)malloc(sizeof(StaticTask_t));
        }
        _console_trace_task_handle = xTaskCreateStaticPinnedToCore(_console_trace_task,
                                TASK_CONSOLE_TRACE_NAME,
                                TASK_CONSOLE_TRACE_STACK,
                                NULL,
                                TASK_CONSOLE_TRACE_PRIORITY,
                                _console_trace_xStack,
                                _console_trace_xTaskBuffer,
                                TASK_CONSOLE_TRACE_RUNNING_CORE);
#else
        xTaskCreatePinnedToCore(_console_trace_task,
                                TASK_CONSOLE_TRACE_NAME,
                                TASK_CONSOLE_TRACE_STACK,
                                NULL,
                                TASK_CONSOLE_TRACE_PRIORITY,
                                &_console_trace_task_handle,
                                TASK_CONSOLE_TRACE_RUNNING_CORE);
#endif
        CONSOLE_TRACE_TASK_TAG_I("Create Task %u", _console_trace_task_handle);
    }
    return _console_trace_task_handle != nullptr;
}

static void _console_trace_task(void *pvParameters) {
    while (_console_trace_task_running) {
        //Receive an item from no-split ring buffer
        size_t item_size;
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
        uint8_t *item = (uint8_t *)xRingbufferReceive(_console_trace_ring_buf_handle, &item_size, portMAX_DELAY);
#else
        uint8_t *item = (uint8_t *)_console_trace_queue.bufferReceive(&item_size, pdMS_TO_TICKS(CONSOLE_TRACE_QUEUE_TIMEOUT_MS), nullptr);
#endif
        if (!item) {
            vTaskDelay(10 / portTICK_PERIOD_MS); // Wait for new data
            continue;
        }

        console_trace_package_t logTrace;
        logTrace.arg = (void*)(*(int*)&item[0]);
        logTrace.log = item + sizeof(void*);
        size_t length = item_size - sizeof(void*);
        CONSOLE_TRACE_TAG_DEBUG("xRingbufferReceive");
        CONSOLE_TRACE_TAG_DEBUG("[Rx]Size = %u", item_size);
        CONSOLE_TRACE_TAG_DEBUG("[Rx]arg = %u", logTrace.arg);
        CONSOLE_TRACE_TAG_DEBUG("[Rx]length = %u", length);
        size_t wLength = 0;
        int wCounterLimit = CONSOLE_TRACE_WRITE_RETRY_CNT;
        while (length > 0 && wCounterLimit >= 0) {
#if (CONSOLE_TRACE_WDT_ENABLE == 1)
            if(esp_task_wdt_add(NULL) != ESP_OK){
                log_e("Failed to add async task to WDT");
            }
#endif
            size_t wLog = ESPConsoleTrace::_s_eventDataHandle(logTrace.arg, logTrace.log + wLength, length);
            length -= wLog;
            wLength += wLog;

#if (CONSOLE_TRACE_WDT_ENABLE == 1)
            if(esp_task_wdt_delete(NULL) != ESP_OK){
                log_e("Failed to remove loop task from WDT");
            }
#endif
            if (0 == length) {
                CONSOLE_TRACE_TAG_DEBUG("Write log succeed");
            } else {
                /** 
                 * User case: File can't open or file write fail.
                */
                --wCounterLimit;
                CONSOLE_TRACE_TAG_DEBUG("Write log failure");
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }
        //Return Item
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)   
        vRingbufferReturnItem(_console_trace_ring_buf_handle, (void *)item);
#else
        _console_trace_queue.bufferReturnItem((void *)item);
#endif
    }

    CONSOLE_TRACE_TASK_TAG_I("task stop");
    CONSOLE_TRACE_TASK_TAG_I("Waiting for delete task");
    if (!task_alloc_delete_ticker(TASK_DELETE_DELAY_MS, [](void *arg)
    {
        if (_console_trace_task_handle != nullptr) {
            TaskHandle_t tempHandle = _console_trace_task_handle;
            /* Pattern B - external delete: vTaskDelete() first, so the TCB is
             * never linked into the idle cleanup list; only then free. */
            vTaskDelete(_console_trace_task_handle);
            _console_trace_task_handle = nullptr;
            _console_trace_queue.end();
            if (_console_trace_xStack) {
                free(_console_trace_xStack);
                _console_trace_xStack = nullptr;
            }
            if (_console_trace_xTaskBuffer) {
                free(_console_trace_xTaskBuffer);
                _console_trace_xTaskBuffer = nullptr;
            }

            CONSOLE_TRACE_TASK_TAG_I("Console trace task cleanup complete for task handle %u", tempHandle);
        }
        /* Option (b): announce "delete done" to the recreate path. */
        task_delete_sem_signal(_console_trace_delete_sem);
    })) {
        /* No mutex in this module (started once from setup), so clearing the
         * handle here can never race a starter. */
        CONSOLE_TRACE_TASK_TAG_E("delete ticker alloc failed: clear handle inline");
        _console_trace_task_handle = nullptr;
        vTaskDelete(NULL); /* no ticker: delete ourselves */
    }

    /* Pattern B: parked until the delete ticker removes us. */
    vTaskDelay(portMAX_DELAY);
}

ESPConsoleTrace::ESPConsoleTrace(Stream *console, bool timeLogOn)
: _stateMutex(xSemaphoreCreateRecursiveMutex()),
  _timeLogOn(timeLogOn) {
    _enable = false;
    _wsEnable = false;
#if (CONSOLE_TRACE_WS_STREAM == 1)
    _consoleWs = nullptr;
    _wsInFlight = nullptr;
#endif
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    _consoleSerial = console;
#endif
#if (CONSOLE_TRACE_FILE_STREAM == 1)
    _consoleFile = nullptr;
    _fs_handle = nullptr;
    _disableFileWrite = false;
#endif
#if (CONSOLE_TRACE_UDP_STREAM == 1)
    _consoleUdp = nullptr;
    _udpPack = nullptr;
#endif
}

ESPConsoleTrace::~ESPConsoleTrace() {
    end();
}

bool ESPConsoleTrace::_stateLock() {
    /* Created on first use as well: the instance is a global object, so its
     * constructor may run before the heap/scheduler is fully up. */
    if (!_stateMutex) {
        _stateMutex = xSemaphoreCreateRecursiveMutex();
        if (!_stateMutex) {
            return false;
        }
    }
    return xSemaphoreTakeRecursive(_stateMutex, portMAX_DELAY) == pdTRUE;
}

void ESPConsoleTrace::_stateUnlock() {
    if (_stateMutex) {
        xSemaphoreGiveRecursive(_stateMutex);
    }
}

#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
void ESPConsoleTrace::beginSerial(Stream *console) {
    _stateLock();
    _consoleSerial = console;
    if (_consoleSerial) {
        _enable = true;
    }
    _stateUnlock();
}
#endif

#if (CONSOLE_TRACE_UDP_STREAM == 1)
void ESPConsoleTrace::beginUdp(int port) {
    if(_udp_handle.listen(port)) {
        CONSOLE_TRACE_TAG_DEBUG("UDP Listening on IP: %s, port: %u", WiFi.localIP().toString().c_str(), port);
        _udp_handle.onPacket([&](AsyncUDPPacket packet) {
            CONSOLE_TRACE_TAG_UDP_DEBUG("UDP Packet Type: %s , From: %s:%u, To: %s:%u, Length: %u", 
                (packet.isBroadcast() ? "Broadcast": packet.isMulticast() ? "Multicast":"Unicast"),
                packet.remoteIP().toString().c_str(),
                packet.remotePort(),
                packet.localIP().toString().c_str(),
                packet.localPort(),
                packet.length()
                );
#if (CONSOLE_TRACE_TAG_UDP_DEBUG_ENABLE == 1)
            CONSOLE_UDP_DEBUG_PORT.printf("Data: ");
            CONSOLE_UDP_DEBUG_PORT.write(packet.data(), packet.length());
            CONSOLE_UDP_DEBUG_PORT.println();
#endif
            if (strcmp((char*)packet.data(), "Disable") == 0) {
                packet.printf("Disable debug\r\n");
                _stateLock();
                _consoleUdp = nullptr;
                if (_udpPack) {
                    AsyncUDPPacket *udpPack = _udpPack;
                    delete udpPack;
                    _udpPack = nullptr;
                    CONSOLE_TRACE_TAG_UDP_DEBUG("Delete current udpPack");
                }
                _stateUnlock();
            }

            if (strcmp((char*)packet.data(), "Enable") == 0) {
                bool enabled = false;
                _stateLock();
                if (_udpPack) {
                    AsyncUDPPacket *udpPack = _udpPack;
                    delete udpPack;
                    _udpPack = nullptr;
                    CONSOLE_TRACE_TAG_UDP_DEBUG("Delete current udpPack");
                }

                _udpPack = new (std::nothrow) AsyncUDPPacket(packet);
                if (_udpPack) {
                    CONSOLE_TRACE_TAG_UDP_DEBUG("Allocated new udpPack");
                    _consoleUdp = static_cast<Stream*>(_udpPack);
                    _enable = true;
                    enabled = true;
                }
                _stateUnlock();
                if (enabled) {
                    packet.printf("Enable debug succeed\r\n");
                }
            }
        });
    }
}
#endif // #if (CONSOLE_TRACE_UDP_STREAM == 1)

#if (CONSOLE_TRACE_FILE_STREAM == 1)
bool ESPConsoleTrace::beginFs(FS &fs, const char* fileName, size_t limitSize) {
    if (!fileName) {
        return false;
    }

    _stateLock();
    /* Drop a previous handle instead of leaking it when beginFs() is called twice. */
    if (_fs_handle) {
        if (*_fs_handle) {
            _fs_handle->close();
        }
        delete _fs_handle;
        _fs_handle = nullptr;
        _consoleFile = nullptr;
    }

    _fs_handle = new (std::nothrow) File;
    if (_fs_handle) {
        CONSOLE_TRACE_TAG_FILE_DEBUG("FS debug recorder file %s", fileName);
        _fs = &fs;
        _limitSize = limitSize;
        _fileName = fileName;
        _disableFileWrite = false;
        *_fs_handle = _fs->open(_fileName, "a");
        if (!(*_fs_handle)) {
            CONSOLE_TRACE_TAG_FILE_DEBUG("Failed to open file %s for writing", _fileName);
            _stateUnlock();
            return false;
        }
        _fs_handle->printf("FS debug recorder start\r\n");
        _consoleFile = static_cast<Stream*>(_fs_handle);
        _enable = true;
    }
    const bool enabled = _enable;
    _stateUnlock();
    return enabled;
}
#endif // #if (CONSOLE_TRACE_FILE_STREAM == 1)

#if (CONSOLE_TRACE_WS_STREAM == 1)
/* Wait for a WebSocket callback that is already running. The caller owns the WS target
 * and may destroy it as soon as closeWs()/wsConsoleSet() return, so it must not return
 * while the console task is still inside the callback. Called from the callback itself
 * (pageTypeSend failed) it returns at once: waiting for ourselves would be pointless. */
void ESPConsoleTrace::_waitWsIdle() {
    const TaskHandle_t self = xTaskGetCurrentTaskHandle();
    for (uint8_t i = 0; i < CONSOLE_TRACE_WS_IDLE_WAIT_MS / 10U; ++i) {
        _stateLock();
        const TaskHandle_t inFlight = _wsInFlight;
        _stateUnlock();
        if (inFlight == nullptr || inFlight == self) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    CONSOLE_TRACE_TAG_DEBUG("Ws console close: a callback is still running");
}

void ESPConsoleTrace::closeWs() {
    if (!_stateLock()) {
        return;
    }
    const bool hadHandler = (_consoleWs != nullptr);
    _consoleWs = nullptr;
    _stateUnlock();

    if (hadHandler) {
        _waitWsIdle();
    }
}

void ESPConsoleTrace::wsConsoleSet(bool wsEnable) {
    if (!_stateLock()) {
        return;
    }
    _wsEnable = wsEnable;
    const bool hadHandler = (_consoleWs != nullptr);
    _consoleWs = nullptr;
    _stateUnlock();

    if (hadHandler) {
        _waitWsIdle();
    }
}
#endif // #if (CONSOLE_TRACE_WS_STREAM == 1)

void ESPConsoleTrace::end() {
    /* Locked for the whole teardown: _addLog() runs under the same mutex, so a handle
     * cannot be deleted while a record is being written to it. */
    _stateLock();
#if (CONSOLE_TRACE_UDP_STREAM == 1)
    if (_consoleUdp) {
        _consoleUdp = nullptr;
        if (_udpPack) {
            AsyncUDPPacket *udpPack = _udpPack;
            delete udpPack;
            _udpPack = nullptr;
        }
    }
#endif

#if (CONSOLE_TRACE_FILE_STREAM == 1)
    if (_consoleFile) {
        _consoleFile = nullptr;
        if (_fs_handle) {
            _fs_handle->close();
            delete _fs_handle;
            _fs_handle = nullptr;
        }
    }
#endif
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    _consoleSerial = nullptr;
#endif
    _enable =  false;
    _stateUnlock();
}

size_t ESPConsoleTrace::_s_eventDataHandle(void *arg, const uint8_t *log, size_t length)  {
    ESPConsoleTrace* handle = reinterpret_cast<ESPConsoleTrace*>(arg);
    return handle->_addLog(log, length);
}

size_t ESPConsoleTrace::_addLog(const uint8_t *buffer, size_t length) {
    if (!buffer || length == 0) {
        return length;
    }

    /* Snapshot the outputs under the state mutex. Serial and the WebSocket handler are
     * then used *outside* it: both take their own locks, and a task holding one of
     * those while logging here would deadlock against us. The file handle is different:
     * end()/consoleFileStop() delete it, so every use of it stays inside the lock, which
     * is what makes the "delete a handle that is being written" crash impossible. */
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    Stream *consoleSerial = nullptr;
#endif
#if (CONSOLE_TRACE_WS_STREAM == 1)
    consoleHandler consoleWs;
    void *wsArg = nullptr;
#endif

    if (!_stateLock()) {
        /* Without the state mutex the outputs cannot be used safely, so the record is
         * dropped instead of being written unlocked. */
        return length;
    }
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    consoleSerial = _consoleSerial;
#endif
#if (CONSOLE_TRACE_WS_STREAM == 1)
    consoleWs = _consoleWs;
    wsArg = _wsArg;
#endif
    const bool enabled = _enable;
    const bool timeLogOn = _timeLogOn;
    _stateUnlock();

    if (!enabled) {
        return length;
    }

    if (timeLogOn && (buffer[0] != 0xFF)) {
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
        if (consoleSerial) {
            String time = ESPTime.toStringConsole();
            consoleSerial->print(time);
            consoleSerial->printf("%010u_%06u, ", millis(), ESP.getFreeHeap());
        }
#endif
        if (_stateLock()) {
#if (CONSOLE_TRACE_UDP_STREAM == 1)
            if (_consoleUdp) {
                String time = ESPTime.toStringConsole();
                static_cast<AsyncUDPPacket*>(_consoleUdp)->print(time);
                // static_cast<AsyncUDPPacket*>(_consoleUdp)->printf("%010u, ", millis());
            }
#endif
#if (CONSOLE_TRACE_FILE_STREAM == 1)
            if (_consoleFile) {
                fs::File* fs_handle = static_cast<fs::File*>(_consoleFile);
                if (!(*fs_handle)) *fs_handle = _fs->open(_fileName, "a");
                if (*fs_handle) {
                    assertOverSize();
                    String time = ESPTime.toStringLog();
                    static_cast<fs::File*>(_consoleFile)->write((const uint8_t*)time.c_str(), time.length());
                    // static_cast<fs::File*>(_consoleFile)->printf("%010u, ", millis());
                }
            }
#endif
            _stateUnlock();
        }
#if (CONSOLE_TRACE_WS_STREAM == 1) && (0) // No need due to WebSerial has been supported
        if (consoleWs) {
            String time = ESPTime.toStringConsole();
            consoleWs(wsArg, (const uint8_t*)time.c_str(), time.length());
        }
#endif
    }
    /**
     * Workaround macro #define CONSOLE_PRINTF(f_, ...) CONSOLE_PORT.printf_P(PSTR("\xff" f_), ##__VA_ARGS__)
     */
    uint8_t offset = 0;
    if (0xFF == buffer[0] && length > 0) {
        buffer++;
        length--;
        offset = 1;
    }

    size_t w = length;
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    if (consoleSerial) {
        w = consoleSerial->write(buffer, length);
    }
#endif
    if (_stateLock()) {
#if (CONSOLE_TRACE_UDP_STREAM == 1)
        if (_consoleUdp) {
            AsyncUDPPacket* packUdp = static_cast<AsyncUDPPacket*>(_consoleUdp);
            w = packUdp->write(buffer, length);
        }
#endif
#if (CONSOLE_TRACE_FILE_STREAM == 1)
        if (_consoleFile) {
            fs::File* fs_handle = static_cast<fs::File*>(_consoleFile);
            if (!(*fs_handle)) *fs_handle = _fs->open(_fileName, "a");
            if (*fs_handle) {
                w = fs_handle->write(buffer, length);
                fs_handle->close(); /* Mandatory writing data to file */
            }
            if (_disableFileWrite) {
                CONSOLE_TRACE_TAG_FILE_DEBUG("File write disabled");
                _disableFileWrite = false;
                _consoleFile = nullptr;
                if (_fs_handle) {
                    if (*_fs_handle) _fs_handle->close();
                    delete _fs_handle;
                    _fs_handle = nullptr;
                }
            }
        }
#endif
        _stateUnlock();
    }
#if (CONSOLE_TRACE_WS_STREAM == 1)
    if (consoleWs) {
        /* Mark the callback as running so closeWs()/wsConsoleSet() wait for it before the
         * caller destroys the WebSocket target. */
        _stateLock();
        _wsInFlight = xTaskGetCurrentTaskHandle();
        _stateUnlock();

        consoleWs(wsArg, buffer, length);

        _stateLock();
        _wsInFlight = nullptr;
        _stateUnlock();
    }
#endif
    return (w + offset);
}

#if (CONSOLE_TRACE_FILE_STREAM == 1)
void ESPConsoleTrace::assertOverSize() {
    if (!_fs_handle || !_fs) {
        return;
    }
    size_t size = _fs_handle->size();
    if (size >= _limitSize) {
#if (CONSOLE_TRACE_WDT_ENABLE == 1)
        esp_task_wdt_reset();
#endif
        CONSOLE_TRACE_TAG_FILE_DEBUG("File size = %lu, limit size = %lu", size, _limitSize);
        _fs_handle->close();
        FSHandle.backup2Folder(*_fs, _fileName);
        *_fs_handle = _fs->open(_fileName, "a");
#if (CONSOLE_TRACE_WDT_ENABLE == 1)
        esp_task_wdt_reset();
#endif
    }
}
#endif

bool ESPConsoleTrace::isEnable() {
    _stateLock();
    if (true
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
        && !_consoleSerial
#endif
#if (CONSOLE_TRACE_UDP_STREAM == 1)
        && !_consoleUdp
#endif
#if (CONSOLE_TRACE_FILE_STREAM == 1)
        && !_consoleFile
#endif
#if (CONSOLE_TRACE_WS_STREAM == 1)
        && !_consoleWs
#endif
    )
    {
        CONSOLE_TRACE_FUNCTION_TAG_DEBUG("All disable console debug");
        _enable = false;
    }
    const bool enabled = _enable;
    _stateUnlock();
    return enabled;
}

// Should be have return size value to prevent crash.
size_t ESPConsoleTrace::write(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    /* Only the flag is read through the state mutex: the queue send below may wait a
     * long time (portMAX_DELAY) and must not hold the state of the object. */
    _stateLock();
    const bool enabled = _enable;
    _stateUnlock();
    if (!enabled) {
        return 0;
    }

    if (isEnable() == false) {
        CONSOLE_TRACE_FUNCTION_TAG_DEBUG("Console trace is not enabled");
        return 0;
    }

    //Send an item
    uint8_t *item = (uint8_t*)malloc(sizeof(void*) + size);
    if (!item) {
        CONSOLE_TRACE_FUNCTION_TAG_DEBUG("Failed to allocate item");
        return 0;
    }
    
    *(int*)&item[0] = (int)this;
    memcpy(item + sizeof(void*), buffer, size);
    CONSOLE_TRACE_TAG_DEBUG("xRingBufferSend");
    CONSOLE_TRACE_TAG_DEBUG("[Tx]Size = %u", sizeof(void*) + size);
    CONSOLE_TRACE_TAG_DEBUG("[Tx]arg = %u(this), %u(arg)", this, *(int*)&item[0]);
#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0)
    UBaseType_t res = xRingbufferSend(_console_trace_ring_buf_handle, item, sizeof(void *) + size, portMAX_DELAY);
    const bool sentOk = (res == pdTRUE);
#else
    /* Q3 / MQ20: bounded publish.  The console trace is a best-effort sink: dropping a line
     * under pressure is strictly better than parking the calling task forever (the old
     * portMAX_DELAY could block any task that logs while the consumer is stopped).
     * N12: compare against the module's own result enum - MQ_OK is numerically pdTRUE, but
     * relying on that is a trap for the next reader. */
    const MqSendResult res = _console_trace_queue.bufferSend(item, sizeof(void *) + size, pdMS_TO_TICKS(100));
    const bool sentOk = (res == MQ_OK);
#endif
    free(item);
    if (!sentOk) {
        CONSOLE_TRACE_FUNCTION_TAG_DEBUG("Failed to send item");
        return 0;
    }
    return size;
}

void ESPConsoleTrace::flush(void)
{
    _stateLock();
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    if (_consoleSerial) {
        _consoleSerial->flush();
    }
#endif
#if (CONSOLE_TRACE_UDP_STREAM == 1)
    if (_consoleUdp) {
        AsyncUDPPacket* packUdp = static_cast<AsyncUDPPacket*>(_consoleUdp);
        packUdp->flush();
    }
#endif
#if (CONSOLE_TRACE_FILE_STREAM == 1)
    if (_consoleFile) {
        fs::File* fs_handle = static_cast<fs::File*>(_consoleFile);
        fs_handle->flush();
    }
#endif
    _stateUnlock();
}

#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
void ESPConsoleTrace::consoleSerialStop() {
    _stateLock();
    _consoleSerial = nullptr;
    _stateUnlock();
}
#endif

#if (CONSOLE_TRACE_FILE_STREAM == 1)
void ESPConsoleTrace::consoleFileStop() {
    _stateLock();
    _disableFileWrite = true;
    _stateUnlock();
}
#endif

#if (CONSOLE_TRACE_UDP_STREAM == 1)
void ESPConsoleTrace::consoleUdpStop() {
    _stateLock();
    if (_consoleUdp) {
        _consoleUdp = nullptr;
        if (_udpPack) {
            AsyncUDPPacket *udpPack = _udpPack;
            delete udpPack;
            _udpPack = nullptr;
        }
    }
    _stateUnlock();
}
#endif

ESPConsoleTrace CONSOLE_TRACE_STREAM;

#endif // #if (gConsoleDebugLevel > 0)
