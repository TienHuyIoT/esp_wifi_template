#ifndef	__ESP_PING_H
#define __ESP_PING_H

#include <Arduino.h>
#include "THIoT_ESPConfig.h"
#if (PING_SERVICE_SYSTEM == 1)
#include "ping/ping.h"
#include "esp_ping.h"
class THIoT_ESPPing
{
private:
    /* data */
    static bool _result;
    static float _avg_time;
    static byte _count;
    static EventGroupHandle_t _ev_handle;
    static void pingResults(ping_target_id_t msgType, struct _ping_found * pf);
public:
    THIoT_ESPPing(/* args */);
    ~THIoT_ESPPing();
    bool ping(IPAddress dest, byte count = 4);
    bool ping(const char *host, byte count = 4);
    float averageTime();
    bool result();
};

#endif // #if (PING_SERVICE_SYSTEM == 1)
#endif // __ESP_PING_H
