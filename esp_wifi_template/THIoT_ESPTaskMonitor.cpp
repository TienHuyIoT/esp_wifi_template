#include "THIoT_ESPTaskMonitor.h"
#include "THIoT_ESPSDFSClass.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFSerialTrace.h"
#include <algorithm>

#define MONITOR_TASK_FS_LOG_SYSTEM          SD_FS_SYSTEM
#define MONITOR_TASK_RECORD_PATH            ((const char*)"/TASK_MONITOR.TXT")
#define MONITOR_TASK_MEMORY_SIZE_LIMITED    (1024 * 1024 * 2)   /* 2Mb */

ESPLogTrace MONITOR_TASK_FS_RECORD(
    MONITOR_TASK_FS_LOG_SYSTEM,
    MONITOR_TASK_RECORD_PATH,
    MONITOR_TASK_MEMORY_SIZE_LIMITED,
    nullptr,
    true,  // Enable time log
    false, // Enable runtime log
    false, // Static task handle
    LOG_TRACE_RECEIVE_QUEUE_TIMEOUT_MS,
    LOG_TRACE_SEND_QUEUE_NO_WAIT,  // No wait for send queue
    LOG_TRACE_FILE_MUTEX_WAIT_MIN, // 1 s acquire budget (macro = 1000U), NOT "no wait" - F15
    LOG_TRACE_QUEUE_TASK_SIZE);    // Static task handle for ESP log trace, it is not used in this case
#define MONITOR_TASK_RECORD_PRINTF(f_, ...)       MONITOR_TASK_FS_RECORD.printf_P(("\xff" f_), ##__VA_ARGS__)
#define MONITOR_TASK_RECORD_PRINTF_LF(f_, ...)    MONITOR_TASK_FS_RECORD.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)

#if (0)
// Print log to console
#define MONITOR_TASK_CONSOLE(...)           CONSOLE_LOGI(__VA_ARGS__)
#define MONITOR_TASK_TAG_CONSOLE(...)       CONSOLE_TAG_LOGI("[TASK_MONITOR]", __VA_ARGS__)
#else
// Print log into SD card
#define MONITOR_TASK_CONSOLE(...)           MONITOR_TASK_RECORD_PRINTF(__VA_ARGS__)
#define MONITOR_TASK_TAG_CONSOLE(...)       MONITOR_TASK_RECORD_PRINTF_LF( __VA_ARGS__)
#endif

void task_hight_water_mark_console(TaskHandle_t taskMonitor) {
    if (taskMonitor == NULL) {
        return;
    }
    if (ESPTaskMonitor.isEnabled() == false) {
        return;
    }
    UBaseType_t size = uxTaskGetStackHighWaterMark(taskMonitor);
    MONITOR_TASK_TAG_CONSOLE("%-20.20s (p=%u) %u bytes",
                             pcTaskGetTaskName(taskMonitor),
                             uxTaskPriorityGet(taskMonitor), size);
}

THIoT_ESPTaskMonitor::THIoT_ESPTaskMonitor() {
#if (MONITOR_TASK_HANDLER == 1)
    _monitor_task_handle = nullptr;
#endif
    _enabled = false;
}

THIoT_ESPTaskMonitor::~THIoT_ESPTaskMonitor()
{
    _enabled = false;
#if (MONITOR_TASK_HANDLER == 1)
    if (_monitor_task_handle) {
        vTaskDelete(_monitor_task_handle);
        _monitor_task_handle = nullptr;
    }
#endif
    ticker_detach(&_monitorTicker);
    _taskHandlers.clear();
}

void THIoT_ESPTaskMonitor::begin(size_t interval) {
    _interval = interval;
    _enabled = true;
#if (MONITOR_TASK_HANDLER == 1)
    if (!_monitor_task_handle) {
        addTask(&_monitor_task_handle);
        addTask(&_i2c_poll_task_handle);
#if (TASK_I2C_DATA_HANDLE_TASK == 1)
        addTask(&_i2c_data_task_handle);
#else
        addTask(&_i2c_pumpLog_task_handle);
        addTask(&_i2c_eventData_task_handle);
#endif
        addTask(&_i2c_frame_task_handle);
        addTask(&_probe_task_handle);
        addTask(&_http_log_task_handle);
        addTask(&_printer_log_task_handle);
        addTask(&_log_trace_task_handle);
        addTask(&_ping_service_task_handle);
        addTask(&_card_cmd_service_task_handle);
#if (CONSOLE_TRACE_TASK_ENABLE == 1)
        addTask(&_console_trace_task_handle);
#endif
        addTask(&_async_service_task_handle);
        addTask(&_lwIP_TaskHandle);
        addTask(&loopTaskHandle);
        xTaskCreateUniversal(_monitor_task, TASK_MONITOR_NAME, TASK_MONITOR_STACK, this, TASK_MONITOR_PRIORITY, &_monitor_task_handle, TASK_MONITOR_RUNNING_CORE);
    }
#else
    MONITOR_TASK_TAG_CONSOLE("Monitor task start");
    ticker_attach_ms(&_monitorTicker, _interval, [](void *arg) {
        THIoT_ESPTaskMonitor *self = static_cast<THIoT_ESPTaskMonitor *>(arg);
        if (self->_enabled) {
            /* Task set of this project: the station specific tasks of web485_v3
             * (i2c / probe / printer / http_log / websocket / ping service ...) do
             * not exist in this template; they are added back when the matching
             * module is ported. */
            self->addTask(&_log_trace_task_handle);
#if (CONSOLE_TRACE_TASK_ENABLE == 1)
            self->addTask(&_console_trace_task_handle);
#endif
            self->addTask(&_lwIP_TaskHandle);
            self->addTask(&loopTaskHandle);
            self->log();
            self->clear();
        }
    }, this);
#endif // #if (MONITOR_TASK_HANDLER == 1)
}

void THIoT_ESPTaskMonitor::addTask(TaskHandle_t *taskHandle) {
    _taskHandlers.push_back(taskHandle);
}

void THIoT_ESPTaskMonitor::removeTask(TaskHandle_t *taskHandle) {
    _taskHandlers.erase(
        std::remove(_taskHandlers.begin(), _taskHandlers.end(), taskHandle),
        _taskHandlers.end()
    );
}

void THIoT_ESPTaskMonitor::clear() {
    _taskHandlers.clear();
}

void THIoT_ESPTaskMonitor::setEnabled(bool enabled) {
    MONITOR_TASK_TAG_CONSOLE("Monitor task %s", enabled ? "ON" : "OFF");
    _enabled = enabled;
}

void THIoT_ESPTaskMonitor::log() {
    MONITOR_TASK_CONSOLE("\r\n");
    for (auto task : _taskHandlers) {
        task_hight_water_mark_console(*task);
    }
    size_t heapFree = ESP.getFreeHeap();
    size_t heapAllocateMax = ESP.getMaxAllocHeap();
    MONITOR_TASK_TAG_CONSOLE("Heap free %u, Heap allocate max %u", heapFree, heapAllocateMax);
    MONITOR_TASK_CONSOLE("\r\n");
}

#if (MONITOR_TASK_HANDLER == 1)
void THIoT_ESPTaskMonitor::_monitor_task(void *pvParameters) {
    MONITOR_TASK_TAG_CONSOLE("Monitor task start");
    THIoT_ESPTaskMonitor *self = static_cast<THIoT_ESPTaskMonitor *>(pvParameters);
    self->monitorLoop();
    MONITOR_TASK_TAG_CONSOLE("Monitor task end");

    MONITOR_TASK_TAG_CONSOLE("Waiting for delete task");
    allocate_ticker_once_ms(TASK_DELETE_DELAY_MS, [](void *arg) {
        THIoT_ESPTaskMonitor *self = static_cast<THIoT_ESPTaskMonitor *>(arg);
        if (self->_monitor_task_handle != nullptr) {
            /* Pattern B - external delete: the ticker deletes the task, so the
             * TCB never enters the idle cleanup list. */
            vTaskDelete(self->_monitor_task_handle);
            self->_monitor_task_handle = nullptr;
            MONITOR_TASK_TAG_CONSOLE("Monitor task handle deleted");
        }
    }, self);

    /* Pattern B: parked until the delete ticker removes us. The two lines that
     * used to follow were unreachable dead code. */
    vTaskDelay(portMAX_DELAY);
}

void THIoT_ESPTaskMonitor::monitorLoop() {
    while (_enabled) {
        vTaskDelay(pdMS_TO_TICKS(_interval));
        log();
    }
}
#endif // #if (MONITOR_TASK_HANDLER == 1)

THIoT_ESPTaskMonitor ESPTaskMonitor;