#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_PFSerialTrace.h"

#ifdef ESP32
#include "esp_system.h"
#define WTD_TAG_CONSOLE(...) //CONSOLE_TAG_LOGI("[WDT]", __VA_ARGS__)

RTC_NOINIT_ATTR int noInitWDT;

static void IRAM_ATTR hw_resetModule(void)
{
    noInitWDT = WDT_IDENTIFY_RESET_NUMBER;
    esp_restart();
}

ESPWatchDogTimer::ESPWatchDogTimer(/* args */)
{
}

ESPWatchDogTimer::~ESPWatchDogTimer()
{
    disable();
}

hw_timer_t* ESPWatchDogTimer::timer = nullptr;

void ESPWatchDogTimer::enable(uint32_t timeout_ms)
{
#if (ESP_IDF_VERSION_MAJOR >= 5)
    timer = timerBegin(1000000);                    // timer 1Mhz resolution
    timerAttachInterrupt(timer, &hw_resetModule);      // attach callback
    timerAlarm(timer, timeout_ms * 1000, false, 0); // set time in us
#else
    timer = timerBegin(0, 80, true);                    // timer 0, div 80
    timerAttachInterrupt(timer, &hw_resetModule, true); // attach callback
    timerAlarmWrite(timer, timeout_ms * 1000, false);   // set time in us
    timerAlarmEnable(timer);                            // enable interrupt
#endif
}
void ESPWatchDogTimer::disable()
{
    if (timer)
    {
        timerEnd(timer);
    }
}

void ESPWatchDogTimer::feed()
{
    if (timer)
    {
        timerWrite(timer, 0); //reset timer (feed watchdog)
    }
}

bool ESPWatchDogTimer::isWdtReset() { return (WDT_IDENTIFY_RESET_NUMBER == noInitWDT); }
void ESPWatchDogTimer::wdtStatusReset() { noInitWDT = WDT_IDENTIFY_INIT_NUMBER; }

ESPWatchDogTimer ESPWdt;
#endif
