#include "THIoT_FactoryButton.h"

FactoryButton::FactoryButton(int button)
// : _button(button, true) //The 2. parameter activeLOW is true
{
}

FactoryButton::~FactoryButton()
{
}

OneButton FactoryButton::_button;

void FactoryButton::onFactory(FactoryButtonHandler handler) {
    _factoryButtonFunc = handler;
}

void FactoryButton::begin() {
    constexpr int TIME_HOLE_BUTTON_MS = 2000;
    _button.setPressTicks(TIME_HOLE_BUTTON_MS); // Duration to hold a button to trigger a long press
    // Fires as soon as the button is held down for n second.
    _button.attachLongPressStart([](void* arg){((FactoryButton*)(arg))->_factoryButtonFunc();}, this); 
#ifdef ESP8266
    _tickButton.attach_ms(50, std::bind(&FactoryButton::loop, this));
#elif defined(ESP32)
    _tickButton.attach_ms(50, static_cast<void (*)(void*)>([](void* arg){((FactoryButton*)(arg))->loop();}), (void*)this);
#endif
}

void FactoryButton::loop() {
    _button.tick();
}