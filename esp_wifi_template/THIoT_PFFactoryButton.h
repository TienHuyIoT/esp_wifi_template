#ifndef __FACTORY_BUTTON_H
#define __FACTORY_BUTTON_H

#include <Arduino.h>
#include <functional>
#include "THIoT_PFTicker.h"
#include <OneButton.h>

typedef std::function<void(void)> FactoryButtonHandler;

class FactoryButton
{
private:
    OneButton _button;
    ticker_function_handle_t _tickButton;
    FactoryButtonHandler _factoryButtonFunc = nullptr;
    void buttonTick();
public:
    FactoryButton(int button);
    ~FactoryButton();
    void onFactory(FactoryButtonHandler handler);
    void begin(int holdMs = 5000);
};

#endif // __FACTORY_BUTTON_H
