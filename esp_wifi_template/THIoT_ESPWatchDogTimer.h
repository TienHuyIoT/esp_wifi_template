#ifndef __ESP_WATCH_DOG_TIMER_H
#define __ESP_WATCH_DOG_TIMER_H

#include <Arduino.h>

#ifdef ESP32
#include "esp32-hal-timer.h"
class ESPWatchDogTimer
{
private:
    static hw_timer_t* timer;
public:
    ESPWatchDogTimer(/* args */);
    ~ESPWatchDogTimer();
    // TODO: figure out how to set WDT timeout
    static void enable(uint32_t timeout_ms = 0);
    static void disable();
    static void feed();

};

extern ESPWatchDogTimer ESPWdt;

#define wdt_enable(time)    ESPWdt.enable(time)
#define wdt_disable()       ESPWdt.disable()
#define wdt_reset()         ESPWdt.feed()

#elif defined(ESP8266)
#include <ESP.h>
#endif

#endif // __ESP_WATCH_DOG_TIMER_H
