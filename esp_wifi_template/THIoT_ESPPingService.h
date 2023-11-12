#ifndef	__ESP_PING_SERVICE_H
#define __ESP_PING_SERVICE_H

#include <Arduino.h>
#include <functional>
#include <ESP32Ping.h>
#include "THIoT_PFTicker.h"

typedef std::function<void(void)> pingServiceHandler;

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
    PingClass _ping;
    pingServiceHandler _succeedFunc;
    pingServiceHandler _failureFunc;
    ticker_function_handle_t _resetTick;
    String _host;
    IPAddress _IP;
    int _interval;
    int _resetCounter;
    int _failCounter;
    int _timeoutReset;
    bool _enable;
    bool _start_ping_task();
public:
    ESPPingService();
    ~ESPPingService();
    TaskHandle_t _ping_poll_task_handle;
    void begin(const char *host, int interval = 15);
    void pingRequest();
    int pingInterval();
    void end();
    void pingStop();
    void pingContinue();
    void errorResetTimeout(int timeout) { _timeoutReset = timeout; }
    void errorResetCounter(int counter) { _failCounter = counter; _timeoutReset = 0;}
    void onSucceed(pingServiceHandler handler) { _succeedFunc = handler; }
    void onFailure(pingServiceHandler handler) { _failureFunc = handler; }
};

#endif // __ESP_PING_SERVICE_H
