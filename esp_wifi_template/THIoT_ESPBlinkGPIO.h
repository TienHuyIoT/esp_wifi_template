#ifndef __ESP_BLINK_GPIO_H
#define __ESP_BLINK_GPIO_H

#include <Arduino.h>
#include <Ticker.h>

typedef struct cycleTimeMs {
public:
    int firstHalf;
    int endHalf;
    cycleTimeMs(int f, int e):firstHalf(f), endHalf(e){}
} cycleTimeMs_t;

class ESPBlinkCycleCallbacks
{
public:
    ~ESPBlinkCycleCallbacks();
    virtual cycleTimeMs_t getCycle(int type);
};

class ESPBlinkGPIO
{
private:
    int8_t _gpio;
    bool _stLevel; // stable level (OFF)
    int _firstHalf_ms;
    int _endHalf_ms;
    int _counter;
    Ticker _longTick;
    Ticker _shortTick;
    ESPBlinkCycleCallbacks* _pCallback;
    void firstHalfLevel();
    void endHalfLevel();
public:
    ESPBlinkGPIO( const bool stLevel, const int8_t gpio = -1);
    ~ESPBlinkGPIO();
    void attach(int firstHalf_ms, int endHalf_ms, int counter = 0 /* Forever */);
    void detach();
    void statusUpdate(int source);
    void setCycleCallbacks(ESPBlinkCycleCallbacks* cb);
    void gpioSet(int8_t gpio);
    void stLevelSet(bool stLevel);
    void activeState();
    void deactiveState();
};

#endif // __ESP_BLINK_GPIO_H
