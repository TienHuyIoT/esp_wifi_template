#include "THIoT_ESPBlinkGPIO.h"
#include "THIoT_SerialTrace.h"

#define BLINK_GPIO_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[BLINK_GPIO]", __VA_ARGS__)

ESPBlinkCycleCallbacks::~ESPBlinkCycleCallbacks(){}

cycleTimeMs_t ESPBlinkCycleCallbacks::getCycle(int type) {
    BLINK_GPIO_TAG_CONSOLE("Callback >> Default <<");
    return cycleTimeMs(100, 900);
}

ESPBlinkGPIO::ESPBlinkGPIO(const int gpio, const bool stLevel)
: _gpio(gpio), _stLevel(stLevel)
{
    pinMode(_gpio, OUTPUT);
    digitalWrite(_gpio, _stLevel);
}

ESPBlinkGPIO::~ESPBlinkGPIO()
{
}

void ESPBlinkGPIO::detach()
{    
    _longTick.detach();
    _shortTick.detach();
    digitalWrite(_gpio, _stLevel);
    BLINK_GPIO_TAG_CONSOLE("stop");
}

void ESPBlinkGPIO::statusUpdate(int source)
{
    cycleTimeMs_t cycle = _pCallback->getCycle(source);
    attach(cycle.firstHalf, cycle.endHalf);
}

void ESPBlinkGPIO::setCycleCallbacks(ESPBlinkCycleCallbacks* cb) 
{
    _pCallback = cb;
}

void ESPBlinkGPIO::attach(int firstHalf_ms, int endHalf_ms, int counter)
{
    BLINK_GPIO_TAG_CONSOLE("Start");
    _counter = _counter;
    _firstHalf_ms = firstHalf_ms;
    _endHalf_ms = endHalf_ms;
    firstHalfLevel(); // called immediately
    // make an interval timeout with total cycle
    _longTick.attach_ms<void*>(_firstHalf_ms + _endHalf_ms, []( void* arg){ 
        ((ESPBlinkGPIO*)(arg))->firstHalfLevel();
    }, this);
}

void ESPBlinkGPIO::firstHalfLevel()
{
    digitalWrite(_gpio, !_stLevel);
    // make an one timeout with the first haft cycle ms
    _shortTick.once_ms<void*>(_firstHalf_ms, []( void* arg){ 
        ((ESPBlinkGPIO*)(arg))->endHalfLevel();
    }, this);
}

void ESPBlinkGPIO::endHalfLevel() 
{
    digitalWrite(_gpio, _stLevel);
    if (_counter > 0)
    {
        if (--_counter == 0)
        {
            // stop cycle blinking GPIO
            _longTick.detach();
            BLINK_GPIO_TAG_CONSOLE("end");
        }
    }
}