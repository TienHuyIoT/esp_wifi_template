#include "THIoT_FactoryButton.h"
#include "THIoT_SerialTrace.h"

#define FACTORY_BUTTON_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[FACTORY_BUTTON]", __VA_ARGS__)

FactoryButton::FactoryButton(int button)
: _button(button, true) //The 2. parameter activeLOW is true
{
}

FactoryButton::~FactoryButton()
{
}

void FactoryButton::onFactory(FactoryButtonHandler handler) {
    _factoryButtonFunc = handler;
}

void FactoryButton::begin() {
    constexpr int TIME_HOLE_BUTTON_MS = 2000;
    _button.setPressTicks(TIME_HOLE_BUTTON_MS); // Duration to hold a button to trigger a long press
    // Fires as soon as the button is held down for n second.
    _button.attachLongPressStart([](void* arg){
            FactoryButtonHandler pCallback = ((FactoryButton*)(arg))->_factoryButtonFunc;
            if (pCallback != nullptr) {
                FACTORY_BUTTON_TAG_CONSOLE("process callback");
                pCallback();
            }
        }
        , this); 
    constexpr int TIME_DEBOUNCE_BUTTON_MS = 100;
    _button.setDebounceTicks(TIME_DEBOUNCE_BUTTON_MS);
    // we should be settup a ticker with interval less than button debounce tick
    _tickButton.attach_ms<void*>(TIME_DEBOUNCE_BUTTON_MS - 20, [](void* arg){((FactoryButton*)(arg))->buttonTick();}, this);
}

void FactoryButton::buttonTick() {
    _button.tick();
}