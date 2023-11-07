#include "THIoT_APPButtonService.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPTimeSystem.h"

#define BUTTON_SERVICE_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[BUTTON_SERVICE]", __VA_ARGS__)

/* Private variables ---------------------------------------------------------*/
const char* const clock_service_id[] PROGMEM = {
  /* 0 */"IO_INPUT_FALLING",
  /* 1 */"IO_INPUT_RISING",
  /* 2 */"BUTTON_SINGER_EVT",
  /* 3 */"BUTTON_DOUBLE_EVT",
  /* 4 */"BUTTON_HOLD_ON_EVT",
  /* 5 */"BUTTON_IDLE_EVT"
};

ButtonServiceClass::ButtonServiceClass(const int pin,
                       const boolean activeLow,
                       const bool pullupActive)
:_button_cxt(IO_INPUT_DEFAULT), _button_service(SERVICE_IO_INPUT_DEFAULT)
,_buttonServiceFunc(nullptr) {
  _pin = pin;

  if (activeLow) {
    _buttonPressed = LOW;
  } 
  else {
    _buttonPressed = HIGH;
  }

  if (pullupActive) {
    pinMode(pin, INPUT_PULLUP);
  } 
  else {
    pinMode(pin, INPUT);
  }
}

ButtonServiceClass::~ButtonServiceClass() {}

void ButtonServiceClass::begin(uint32_t tAlarm) {
    constexpr uint32_t instance = 20231026;
    _button_service.instance = instance;
    _button_service.level_active = _buttonPressed;
    /* Set around 1min to active rising edged */
    _button_service.single_time_trigger = INPUT_SINGLE_TIME_TRIGGER;
    _button_service.hold_time_trigger   = INPUT_HOLD_TIME_TRIGGER;
    _button_service.next_time_trigger   = INPUT_NEXT_TIME_TRIGGER;
    _button_service.idle_time_trigger   = INPUT_IDLE_TIME_TRIGGER;

    /** 5 is high sample rate count
     * 5 is low sample rate count
     * That's mean, the button shall be active when
     * 5 low level and 5 high level
     * Refer clockTick() to check High and Low level
    */
    io_input_init(&_button_cxt, 5, 5);
    service_io_input_init(&_button_cxt, &_button_service);
    service_io_input_ev_register(
        &_button_service,
        [](service_io_input_handle_t *service, uint8_t evt, void *arg)
        {
            ButtonServiceClass *handler = (ButtonServiceClass *)arg;
            handler->serviceHandler(service, evt);
        },
        this);

    /** Init tick sample rate 20ms */
    ticker_attach_ms(&_clockTick, 20, [](void* arg) {
        ButtonServiceClass *handler = (ButtonServiceClass *)arg;
        if (handler->_pin >= 0) {
          io_input_process(&handler->_button_cxt, digitalRead(handler->_pin));
        }
    }, this);
}

void ButtonServiceClass::serviceHandler(service_io_input_handle_t *service, uint8_t evt) {
  BUTTON_SERVICE_TAG_CONSOLE("Button service %u: %s", service->instance, FPSTR(clock_service_id[evt]));
  if (service == &_button_service) {
    if (_buttonServiceFunc) {
          _buttonServiceFunc(evt);
      }
    switch (evt) {
      case IO_INPUT_FALLING:
        break;

      case IO_INPUT_RISING:
        break;

      case BUTTON_SINGER_EVT:
        break;

      case BUTTON_DOUBLE_EVT:
        break;

      case BUTTON_HOLD_ON_EVT:
        break;

      case BUTTON_IDLE_EVT:
        break;

      default:
        break;
    }
  }
}
