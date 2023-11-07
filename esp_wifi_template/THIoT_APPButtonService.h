#ifndef __ALARM_CLOCK_H
#define __ALARM_CLOCK_H

#include <Arduino.h>
#include <functional>
#include "THIoT_PFTicker.h"
#include "THIoT_PFInputService.h"

typedef std::function<void(uint8_t evt)> ButtonServiceHandler;

class ButtonServiceClass
{
private:
    ticker_function_handle_t _clockTick;
    io_input_cxt_t _button_cxt;
    service_io_input_handle_t _button_service;
    ButtonServiceHandler _buttonServiceFunc;
    uint32_t _tStart;
    uint32_t _tAlarm;
    uint32_t _tStop;
    int _pin;
    uint8_t _buttonPressed;
    void calculatorTimeOffset(uint32_t tAlarm);
    void serviceHandler(service_io_input_handle_t *service, uint8_t evt);
public:
    ButtonServiceClass(const int pin,
                       const boolean activeLow = true,
                       const bool pullupActive = true);
    ~ButtonServiceClass();
    void onAlarm(ButtonServiceHandler handler) { _buttonServiceFunc = handler; };
    void begin(uint32_t tAlarm);
};

#endif // __ALARM_CLOCK_H
