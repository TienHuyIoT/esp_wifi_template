#ifndef __FACTORY_BUTTON_H_
#define __FACTORY_BUTTON_H_

#include <Arduino.h>
#include <functional>
#include <Ticker.h>
#include <OneButton.h>

typedef std::function<void(void)> FactoryButtonHandler;

class FactoryButton
{
private:
    OneButton _button;
    Ticker _tickButton;
    FactoryButtonHandler _factoryButtonFunc = nullptr;
    void buttonTick();
public:
    FactoryButton(int button);
    ~FactoryButton();
    void onFactory(FactoryButtonHandler handler);
    void begin();
};

#endif // __FACTORY_BUTTON_H
