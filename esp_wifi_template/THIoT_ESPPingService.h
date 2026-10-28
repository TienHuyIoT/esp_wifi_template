#ifndef	__ESP_PING_SERVICE_H
#define __ESP_PING_SERVICE_H

#include <Arduino.h>
#include <functional>
#include <Ticker.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_APPConfig.h"
#include "THIoT_PFTicker.h"
#if (PING_SERVICE_SYSTEM == 1)
#include "THIoT_ESPPing.h"
#else
#include <ESP32Ping.h>
#endif

/** Run 1 task instead of creation the multitasks. */
#define PING_SINGLE_TASK_HANDLER    1

typedef enum {
    PING_SERVICE_SEND,
    PING_SERVICE_RECV,
    PING_SERVICE_FIN,
    PING_SERVICE_ERROR
} ping_service_t;

typedef std::function<void(const char *host, int count, float time)> pingServiceHandler;

class ESPPingService
{
public:
    typedef enum {
        PING_GATEWAY_HOST = 0,
        PING_INTERNET_HOST
    } pingHost_t;
    typedef enum {
        PING_ERROR_NONE = 0,
        PING_ERROR_RESET
    } pingType_t;

private:
#if (PING_SERVICE_SYSTEM == 1)
    THIoT_ESPPing _ping;
#else
    PingClass _ping;
#endif
    pingServiceHandler _succeedFunc;
    pingServiceHandler _failureFunc;
    Ticker _intervalTicker;
    String _host;
    int _interval;
    int _resetCounter;
    int _failCounter;
    int _timeoutReset;
    bool _enable;
    bool _putRequesttoQueue(ping_service_t *log);

public:
    ESPPingService();
    ~ESPPingService();
    static bool start_ping_task();

    void begin(const char *host, int interval = 15);
    void end();
    void pingRequest();
    void errorResetTimeout(int timeout) { _timeoutReset = timeout; }
    void errorResetCounter(int counter) { _failCounter = counter; _timeoutReset = 0;}
    void onSucceed(pingServiceHandler handler) { _succeedFunc = handler; }
    void onFailure(pingServiceHandler handler) { _failureFunc = handler; }

    int pingInterval() {
        return _interval;
    }

    void pingStop() {
        _enable = false;
    }

    void pingContinue() {   
        _enable = true;
    }

    // Static methods for task handling
    static void _s_eventDataHandle(void *arg, ping_service_t *log);
};

#endif // __ESP_PING_SERVICE_H
