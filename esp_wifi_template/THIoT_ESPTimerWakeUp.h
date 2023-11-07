#ifndef __ESP_TIMER_WAKEUP_H
#define __ESP_TIMER_WAKEUP_H

#include <Arduino.h>

String esp_wakeup_reason_str();
void timer_wakeup_second_enable(uint32_t sec);

#endif // __ESP_TIMER_WAKEUP_H