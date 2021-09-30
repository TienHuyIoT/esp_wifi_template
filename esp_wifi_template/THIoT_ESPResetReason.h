#ifndef __ESP_RESET_REASON_H
#define __ESP_RESET_REASON_H

#include <Arduino.h>

#ifdef ESP32
#if (ESP_IDF_VERSION_MAJOR >= 4) // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include <esp32/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32S2
#include <esp32s2/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32C3
#include <esp32c3/rom/rtc.h>
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/rtc.h"
#endif
#elif defined(ESP8266)
#include <ESP.h>
#endif

#ifdef ESP32
uint32_t esp_reset_reason(int cpu_no);
#elif defined(ESP8266)
uint32_t esp_reset_reason();
#endif
String esp_reset_reason_str();

#endif // __ESP_RESET_REASON_H
