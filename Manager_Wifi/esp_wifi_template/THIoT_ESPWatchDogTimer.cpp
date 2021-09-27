#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_SerialTrace.h"

#ifdef ESP32
#include "esp_system.h"
#define WTD_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[WDT]", __VA_ARGS__)

static void IRAM_ATTR hw_resetModule(void)
{
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
    timer = timerBegin(0, 80, true);                  //timer 0, div 80
    timerAttachInterrupt(timer, &hw_resetModule, true);  //attach callback
    timerAlarmWrite(timer, timeout_ms * 1000, false); //set time in us
    timerAlarmEnable(timer);                          //enable interrupt
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

ESPWatchDogTimer ESPWdt;
#endif
