#ifndef __ESP_LED_STATUS_H
#define __ESP_LED_STATUS_H

#include <Arduino.h>
#include "THIoT_ESPBlinkGPIO.h"
#include "THIoT_SerialTrace.h"
#define LED_STATUS_TAG_CONSOLE(...) SERIAL_FUNCTION_TAG_LOGI("[LED_STATUS]", __VA_ARGS__)
class ESPLedCycleBlinkCallbacks : public ESPBlinkCycleCallbacks
{
public:
    ~ESPLedCycleBlinkCallbacks(){}
    enum : uint8_t {
        BLINK_NORMAL = 0,
        BLINK_WIFI_GOT_IP,
        BLINK_WIFI_DISCONNECTED,
        BLINK_ETH_GOT_IP,
        BLINK_ETH_DISCONNECTED,
        BLINK_FACTORY_SYSTEM_PARAMS
    };

    cycleTimeMs_t getCycle(int type) override {
        LED_STATUS_TAG_CONSOLE("callback");
        cycleTimeMs_t cycleTime(100 /* On 100ms */, 1900 /* Off 1900ms */);
        switch (type)
        {
        case BLINK_NORMAL:
        case BLINK_ETH_DISCONNECTED:
        case BLINK_WIFI_DISCONNECTED:
            cycleTime = cycleTimeMs(100, 100);
            break;
        case BLINK_WIFI_GOT_IP:
            cycleTime = cycleTimeMs(100, 900);
            break;
        case BLINK_ETH_GOT_IP:
            cycleTime = cycleTimeMs(100, 400);
            break;
        case BLINK_FACTORY_SYSTEM_PARAMS:
            cycleTime = cycleTimeMs(1000, 1000);
            break;
        default:
            break;
        }
        return cycleTime;
    }
};

#endif // __ESP_LED_STATUS_H