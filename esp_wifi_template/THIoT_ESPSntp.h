#ifndef	__ESP_SNTP_H
#define __ESP_SNTP_H

#include <Arduino.h>
#include "THIoT_PFTicker.h"
class ESPSntpService
{
private:
    static String _server1;
    static String _server2;
    static String _timeZone;
    static long _gtmOffset;
    static int _daylightOffset;
    static bool syncRestart();

public:
    ESPSntpService(/* args */);
    ~ESPSntpService();
    void begin();
    static boolean _syncEvent;
    static ticker_function_handle_t _interval;
    static ticker_function_handle_t _event;
    static boolean syncEventStatus() { return _syncEvent; }
};

#endif // __ESP_SNTP_H
