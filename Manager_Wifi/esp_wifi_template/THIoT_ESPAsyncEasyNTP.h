#ifndef __ESP_ASYNC_EASY_NTP_H
#define __ESP_ASYNC_EASY_NTP_H

#include <Arduino.h>
#ifdef ESP32
#include "AsyncUDP.h"
#elif defined(ESP8266)
#include <ESPAsyncUDP.h>
#endif

#define NTP_REQUEST_PORT   123
#define NTP_PACKET_SIZE    48 // NTP timestamp is in the first 48 bytes of the message

typedef std::function<void (struct timeval*)> onSyncEvent_t;

class ESPAsyncEasyNTP
{
private:
    onSyncEvent_t _onSyncEvent;
    String _server;
    AsyncUDP _udp;
    IPAddress _ipServer;
    void setTimeZone(long offset, int daylight);
    void sendNTPpacket();
    void _onPacket(AsyncUDPPacket& packet);
public:
    ESPAsyncEasyNTP(/* args */);
    ~ESPAsyncEasyNTP();

    void begin(long gmtOffset = (3600 * 7), int daylightOffset = 0, const char *server = "time.nist.gov");
    
    void onNTPSyncEvent (onSyncEvent_t handler) {
        _onSyncEvent = handler;
    }

    void runAsync();    
    void end();
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_EASYNTP)
extern ESPAsyncEasyNTP EASYNTP;
#endif

#endif // __ESP_ASYNC_EASY_NTP_H