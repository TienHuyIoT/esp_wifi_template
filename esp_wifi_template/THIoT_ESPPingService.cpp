#include <Esp.h>
#include <Arduino.h>
#include <atomic>
#include <Ticker.h>
#include <ArduinoJson.h>
#include "THIoT_PFTicker.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPPingService.h"
#include "THIoT_ESPTaskConfig.h"
#include "THIoT_ESPTaskHelper.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define PING_SERVICE_TAG_CONSOLE_E(...) CONSOLE_TAG_LOGE("[PING]", __VA_ARGS__)
#define PING_SERVICE_TAG_CONSOLE_I(...) CONSOLE_TAG_LOGI("[PING]", __VA_ARGS__)

#define PING_SERVICE_TAG_LOG(...)       FS_TAG_LOGI("[PING]", __VA_ARGS__)

#define PING_QUEUE_SIZE             3U
#define PING_INTERVAL_MIN           60U // Default ping interval in seconds
typedef struct {
    ping_service_t service;
    void *arg;
} ping_service_package_t;

static xQueueHandle _ping_queue = nullptr;
static SemaphoreHandle_t _ping_service_task_mutex = nullptr;
bool _ping_service_task_running = false;
TaskHandle_t _ping_service_task_handle = nullptr;
#if (TASK_DELETE_USING_TICKER == 1)
Ticker _ping_service_tick;
#endif
#if (TASK_PING_SERVICE_STATIC == 1)
StackType_t  *_ping_service_xStack;
StaticTask_t *_ping_service_xTaskBuffer;
#endif

static inline bool _ping_service_queue_init()
{
    if (!_ping_queue) {
        _ping_queue = xQueueCreate(PING_QUEUE_SIZE, sizeof(ping_service_package_t));
        if (!_ping_queue) {
            PING_SERVICE_TAG_CONSOLE_E("Failed to create ping queue");
            return false;
        }
    }
    return true;
}

static inline bool _send_ping_service_event(ping_service_package_t *evt)
{
    _ping_service_queue_init();
    return _ping_queue && xQueueSend(_ping_queue, evt, pdMS_TO_TICKS(0)) == pdPASS;
}

static inline bool _get_ping_service_event(ping_service_package_t *evt)
{
    return _ping_queue && xQueueReceive(_ping_queue, evt, pdMS_TO_TICKS(0) /* portMAX_DELAY */) == pdPASS;
}

static void _ping_service_task(void *pvParameters)
{
    ping_service_package_t evt;
    _ping_service_task_running = true;
    for (;;)
    {
        if (!_get_ping_service_event(&evt)) {
            PING_SERVICE_TAG_CONSOLE_E("no event");
            break; // Exit if we cannot get the event
        }
        ESPPingService::_s_eventDataHandle(evt.arg, &evt.service);
    }

    _ping_service_task_running = false;
    PING_SERVICE_TAG_CONSOLE_I("task stop");
    PING_SERVICE_TAG_CONSOLE_I("Waiting for delete task");
    /* Pattern B - external delete. The ticker MUST delete the task itself:
     * the task body does NOT call vTaskDelete(NULL), so the TCB never enters
     * xTasksWaitingTermination and the idle task never touches it. That is what
     * makes it safe to free the static TCB/stack in this same callback - with a
     * self delete the idle task would still be about to unlink the TCB, and
     * freeing it there faults the device (LoadProhibited in vTaskSwitchContext,
     * core 0). Here the callback runs after vTaskDelete() has already made the
     * task dormant, so the free is ordered. */
    ticker_function_handle_t *delete_ticker = allocate_ticker_once_ms(TASK_DELETE_DELAY_MS, [](void *arg)
    {
        xSemaphoreTake(_ping_service_task_mutex, portMAX_DELAY);
        if (_ping_service_task_handle != nullptr) {
            TaskHandle_t tmp_handle = _ping_service_task_handle;
            vTaskDelete(_ping_service_task_handle);
            _ping_service_task_handle = nullptr;
            if (_ping_service_xTaskBuffer) {
                free(_ping_service_xTaskBuffer);
                _ping_service_xTaskBuffer = nullptr;
            }
            if (_ping_service_xStack) {
                free(_ping_service_xStack);
                _ping_service_xStack = nullptr;
            }
            PING_SERVICE_TAG_CONSOLE_I("ping service task deleted: %u", (uint32_t)tmp_handle);
        }
        xSemaphoreGive(_ping_service_task_mutex);
    }, nullptr);
    if (!delete_ticker) {
        /**
         * Failed to allocate the delete ticker, which means the task may not be properly deleted.
         * We clear the handle so the recreate path does not stall on a task that will
         * never be cleared. The buffer and stack are kept for potential reuse.
        */
        PING_SERVICE_TAG_CONSOLE_E("delete ticker alloc failed: clear handle inline");
        task_delete_fallback(_ping_service_task_mutex, &_ping_service_task_handle);
        vTaskDelete(NULL); /* no ticker: delete ourselves */
    }
    /* Pattern B: park here and let the ticker delete us; never self delete on
     * the success path, otherwise the TCB enters the idle cleanup list and the
     * free in the ticker would race it. */
    vTaskDelay(portMAX_DELAY);
}

static bool _start_ping_task() {
    if (!_ping_service_task_mutex) {
        _ping_service_task_mutex = xSemaphoreCreateMutex();
        if (!_ping_service_task_mutex) {
            return false;
        }
    }

    if (!_ping_queue) {
        _ping_queue = xQueueCreate(PING_QUEUE_SIZE, sizeof(ping_service_package_t));
        if (!_ping_queue) {
            return false;
        }
    }

    if (_ping_service_task_handle == NULL || !_ping_service_task_running) {
        /**
         * NOTE - option (b) deliberately NOT used here (same-task case).
         *
         * This recreate path is called from _intervalTicker, and the clearer that
         * clears _ping_service_task_handle is another ticker callback. Ticker
         * callbacks all run in loopTask (THIoT_PFTicker.cpp ->
         * ticker_schedule_handler(), whose ONLY caller is loop() in
         * web485_v3.ino), so the clearer cannot run while this path waits. With
         * the endless wait of task_wait_pending_delete() that would DEADLOCK the
         * loopTask, not merely expire at a cap.
         *
         * It is safe to skip the wait: TASK_CREATE_DELAY_MS (200 ms) >
         * TASK_DELETE_DELAY_MS (10 ms), and the recreate only happens on the next
         * interval tick (>= PING_INTERVAL_MIN = 60 s), long after the clearer ran.
         *
         * The modules whose recreate path runs on another task (i2c_data, ...)
         * use task_wait_pending_delete() from THIoT_ESPTaskHelper.h instead.
         */
        xSemaphoreTake(_ping_service_task_mutex, portMAX_DELAY);
        if (_ping_service_task_handle == NULL)
        {
            PING_SERVICE_TAG_CONSOLE_I("Creating ping service task handle...");
#if (TASK_PING_SERVICE_STATIC == 1)
            if (!_ping_service_xTaskBuffer) {
                _ping_service_xTaskBuffer = (StaticTask_t *)malloc(sizeof(StaticTask_t));
            }
            if (!_ping_service_xStack) {
                _ping_service_xStack = (StackType_t *)malloc(TASK_PING_SERVICE_STACK);
            }
            _ping_service_task_handle = xTaskCreateStaticPinnedToCore(_ping_service_task,
                                                                      TASK_PING_SERVICE_NAME,
                                                                      TASK_PING_SERVICE_STACK,
                                                                      NULL,
                                                                      TASK_PING_SERVICE_PRIORITY,
                                                                      _ping_service_xStack,
                                                                      _ping_service_xTaskBuffer,
                                                                      TASK_PING_SERVICE_RUNNING_CORE);
#else
            xTaskCreatePinnedToCore(_ping_service_task,
                                    TASK_PING_SERVICE_NAME,
                                    TASK_PING_SERVICE_STACK,
                                    NULL,
                                    TASK_PING_SERVICE_PRIORITY,
                                    &_ping_service_task_handle,
                                    TASK_PING_SERVICE_RUNNING_CORE);
#endif
            PING_SERVICE_TAG_CONSOLE_I("Ping service task handle created: %u", _ping_service_task_handle);
        }
        xSemaphoreGive(_ping_service_task_mutex);
    }
    return _ping_service_task_handle != NULL;
}

ESPPingService::ESPPingService()
:_ping() {
    _failCounter = 0;
    _resetCounter = 0;
    _timeoutReset = 0;
}

ESPPingService::~ESPPingService() {
    end();
}

void ESPPingService::end() {
    _enable = false;
    _intervalTicker.detach();
}

void ESPPingService::begin(const char *host, int interval) {
    _enable = true;
    _host = host;
    _resetCounter = 0;

    _interval = interval;
    if (_interval < PING_INTERVAL_MIN) {
        _interval = PING_INTERVAL_MIN;
    }
    if (_timeoutReset > 0) {
        _failCounter = _timeoutReset / _interval;
    }
    PING_SERVICE_TAG_CONSOLE_I("Interval = %u", _interval);
    PING_SERVICE_TAG_CONSOLE_I("failCounter = %u", _failCounter);
    PING_SERVICE_TAG_CONSOLE_I("host = %s", _host.c_str());

    _intervalTicker.attach<void*>(_interval, [](void *arg) {
        ESPPingService *self = (ESPPingService*)arg;
        ping_service_t service = PING_SERVICE_SEND;
        if (!self->_putRequesttoQueue(&service)) {
            PING_SERVICE_TAG_CONSOLE_E("Failed to queue ping request");
        }
        // Dynamically start the ping task if not already running
        _start_ping_task();
    }, this);

#if (0) // just for test
    // Start the ping service task immediately at the beginning
    allocate_ticker_once_ms(1000, [](void *arg) {
        ESPPingService *self = (ESPPingService*)arg;
        ping_service_t service = PING_SERVICE_SEND;
        if (!self->_putRequesttoQueue(&service)) {
            PING_SERVICE_TAG_CONSOLE_E("Failed to queue ping request");
        }
        // Dynamically start the ping task if not already running
        _start_ping_task();
    }, this);
#endif
}

bool ESPPingService::_putRequesttoQueue(ping_service_t *service) {
    if (!_enable) {
        PING_SERVICE_TAG_CONSOLE_I("Disable");
        return false;
    }

    ping_service_package_t evt;
    evt.arg = this;
    evt.service = *service;
    return _send_ping_service_event(&evt);
}

void ESPPingService::pingRequest() {
    bool status;
    if (!_enable) {
        PING_SERVICE_TAG_CONSOLE_I("Disable");
        return;
    }
    if (_host.isEmpty()) {
        PING_SERVICE_TAG_CONSOLE_E("Host is empty");
        return;
    }
    PING_SERVICE_TAG_CONSOLE_I("Start Ping %s", _host.c_str());
    IPAddress ip;
    if (ip.fromString(_host.c_str())) {
        PING_SERVICE_TAG_CONSOLE_I("Ping IP: %s", ip.toString().c_str());
        status = _ping.ping(ip, 4);  /* 4: Ping times */
    } else {
        PING_SERVICE_TAG_CONSOLE_I("Ping Host: %s", _host.c_str());
        status = _ping.ping(_host.c_str(), 4);  /* 4: Ping times */
    }
#if (PING_SERVICE_SYSTEM == 1)
    if (status && _ping.result()) 
#else
    if (status) 
#endif
    {
        PING_SERVICE_TAG_CONSOLE_I("Ping %s succeed with averageTime = %.1fms", _host.c_str(), _ping.averageTime());
        if (_succeedFunc) {
            _succeedFunc(_host.c_str(), 4, _ping.averageTime());
        }
        _resetCounter = 0;
    }
    else {
        PING_SERVICE_TAG_LOG("Ping %s Error", _host.c_str());
        if (_failureFunc) {
            _failureFunc(_host.c_str(), 4, _ping.averageTime());
        }

        if (_failCounter > 0) {
            ++_resetCounter;
            if (_resetCounter >= _failCounter) {
                _resetCounter = 0;
                PING_SERVICE_TAG_LOG("Reset due to Ping error");
                SOFTReset.enable(500, ESPSoftReset::PING_GATEWAY_FAIL_TYPE);
            }
        }
    }
}

void ESPPingService::_s_eventDataHandle(void *arg, ping_service_t *service) {
    ESPPingService* handle = reinterpret_cast<ESPPingService*>(arg);
    handle->pingRequest();
}

bool ESPPingService::start_ping_task() {
    return _start_ping_task();
}