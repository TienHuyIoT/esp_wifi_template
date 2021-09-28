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
    static OneButton _button;
    Ticker _tickButton;
    FactoryButtonHandler _factoryButtonFunc = nullptr;
public:
    FactoryButton(int button);
    ~FactoryButton();
    void onFactory(FactoryButtonHandler handler);
    void begin();
    void loop();
};

#endif // __FACTORY_BUTTON_H
