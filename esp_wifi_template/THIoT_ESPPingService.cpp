#include <Esp.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPPingService.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define PING_TAG_CONSOLE(...)   SERIAL_TAG_LOGI("[PING]", __VA_ARGS__)
#define PING_TAG_LOG(...)       FS_TAG_LOGI("[PING]", __VA_ARGS__)

#define CONFIG_PING_QUEUE_SIZE  5
#define PING_RUNNING_CORE0      0

typedef enum{
    LWIP_PING_SENT,
    LWIP_PING_RECV,
    LWIP_PING_FIN,
    LWIP_PING_ERROR
} lwip_ping_t;

typedef struct {
    lwip_ping_t event;
    void *arg;
} lwip_ping_packet_t;

static xQueueHandle _ping_queue;

static inline bool _init_ping_event_queue(){
    if(!_ping_queue){
        _ping_queue = xQueueCreate(CONFIG_PING_QUEUE_SIZE, sizeof(lwip_ping_packet_t *));
        if(!_ping_queue){
            return false;
        }
    }
    return true;
}

static inline bool _send_async_event(lwip_ping_packet_t ** e){
    return _ping_queue && xQueueSend(_ping_queue, e, portMAX_DELAY) == pdPASS;
}

static inline bool _prepend_async_event(lwip_ping_packet_t ** e){
    return _ping_queue && xQueueSendToFront(_ping_queue, e, portMAX_DELAY) == pdPASS;
}

static inline bool _get_async_event(lwip_ping_packet_t ** e){
    return _ping_queue && xQueueReceive(_ping_queue, e, portMAX_DELAY) == pdPASS;
}

static void _handle_async_event(lwip_ping_packet_t * e) {
    if(e->arg == NULL){
        // do nothing when arg is NULL
        //ets_printf("event arg == NULL: 0x%08x\n", e->recv.pcb);
    } else if(e->event == LWIP_PING_SENT){
         
    } else if(e->event == LWIP_PING_RECV){
         
    } else if(e->event == LWIP_PING_FIN){
         
    } else if(e->event == LWIP_PING_ERROR){
         
    }
    free((void*)(e));
}

static void _task_ping_poll(void *pvParameters) {
    ESPPingService *handle = reinterpret_cast<ESPPingService*>(pvParameters);
    for (;;) {
        handle->pingRequest();
        vTaskDelay(handle->pingInterval() * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(handle->_ping_poll_task_handle);
}

ESPPingService::ESPPingService()
:_ping() {
    _ping_poll_task_handle = NULL;
    _failCounter = 0;
    _resetCounter = 0;
    _timeoutReset = 0;
}

ESPPingService::~ESPPingService() {
    end();
}

void ESPPingService::end() {
    if (_ping_poll_task_handle) {
        PING_TAG_CONSOLE("vTaskDelete %lu", _ping_poll_task_handle);
        vTaskDelete(_ping_poll_task_handle);
        _ping_poll_task_handle = NULL;
    }
}

void ESPPingService::begin(const char *host, int interval) {
    _host = host;
    _interval = interval;
    _resetCounter = 0;
    if (_timeoutReset > 0) {
        _failCounter = _timeoutReset / _interval;
    }
    PING_TAG_CONSOLE("Interval = %u", _interval);
    PING_TAG_CONSOLE("resetCounter = %u", _resetCounter);
    PING_TAG_CONSOLE("failCounter = %u", _failCounter);
    PING_TAG_CONSOLE("host = %s", _host.c_str());
    _start_ping_task();
}

void ESPPingService::pingRequest() {
    bool status;
    status = _ping.ping(_host.c_str(), 1);
    if (status) {
        PING_TAG_CONSOLE("Ping %s succeed with averageTime = %.2fms", _host.c_str(), _ping.averageTime());
        if (_succeedFunc) {
            _succeedFunc();
        }
        _resetCounter = 0;
    }
    else {
        PING_TAG_CONSOLE("Ping %s Error", _host.c_str());
        if (_failureFunc) {
            _failureFunc();
        }

        if (_failCounter > 0) {
            ++_resetCounter;
            if (_resetCounter >= _failCounter) {
                _resetCounter = 0;
                PING_TAG_CONSOLE("Reset due to Ping error");
                PING_TAG_LOG("Reset due to Ping error");
                SOFTReset.enable(500, ESPSoftReset::PING_GATEWAY_FAIL_TYPE);
            }
        }
    }
}

int ESPPingService::pingInterval() {
    return _interval;
}

bool ESPPingService::_start_ping_task() {
    if (!_ping_poll_task_handle) {
        xTaskCreatePinnedToCore(
            _task_ping_poll, "Task ping poll", 1024 * 3 // Stack size
            , this // When no parameter is used, simply pass NULL
            , 2  // Priority
            , &_ping_poll_task_handle // With task handle we will be able to manipulate with this task.
            , PING_RUNNING_CORE0 // Core on which the task will run
        );
        if (!_ping_poll_task_handle) {
            return false;
        }
        PING_TAG_CONSOLE("Create Task %lu", _ping_poll_task_handle);
    }
    return true;
}