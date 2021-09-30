#ifndef __SERIAL_TRACE_H
#define __SERIAL_TRACE_H

#include <Arduino.h>

#if (0)
#include <iostream>
#include <string_view>
#include <source_location>
 
void log(const std::string_view message,
         const std::source_location location = 
               std::source_location::current())
{
    std::cout << "file: "
              << location.file_name() << "("
              << location.line() << ":"
              << location.column() << ") `"
              << location.function_name() << "`: "
              << message << '\n';
}
#endif
#define SERIAL_PORT Serial
#define SERIAL_BAUDRATE 921600
#define SERIAL_FLUSH() (SERIAL_PORT.flush())
#define SERIAL_PRINTF(f_, ...) SERIAL_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)
#define SERIAL_PRINTFLN(f_, ...) SERIAL_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)
#define SERIAL_TAG_PRINTF(tag, f_, ...)  SERIAL_PORT.printf_P(PSTR(tag f_), ##__VA_ARGS__)
#define SERIAL_FUNCTION_PRINTFLF(f_, ...)  SERIAL_PORT.printf_P(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)

#define gSerialDebugLevel 4

#define SERIAL_LOGE(...) do{if(gSerialDebugLevel > 0) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)
#define SERIAL_LOGW(...) do{if(gSerialDebugLevel > 1) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)
#define SERIAL_LOGI(...) do{if(gSerialDebugLevel > 2) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)
#define SERIAL_LOGD(...) do{if(gSerialDebugLevel > 3) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)
#define SERIAL_LOGV(...) do{if(gSerialDebugLevel > 4) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)

#define SERIAL_TAG_LOGE(x, ...) do{if(gSerialDebugLevel > 0) {SERIAL_PRINTFLN("E " x ": " __VA_ARGS__);}}while(0)
#define SERIAL_TAG_LOGW(x, ...) do{if(gSerialDebugLevel > 1) {SERIAL_PRINTFLN("W " x ": " __VA_ARGS__);}}while(0)
#define SERIAL_TAG_LOGI(x, ...) do{if(gSerialDebugLevel > 2) {SERIAL_PRINTFLN("I " x ": " __VA_ARGS__);}}while(0)
#define SERIAL_TAG_LOGD(x, ...) do{if(gSerialDebugLevel > 3) {SERIAL_PRINTFLN("D " x ": " __VA_ARGS__);}}while(0)
#define SERIAL_TAG_LOGV(x, ...) do{if(gSerialDebugLevel > 4) {SERIAL_PRINTFLN("V " x ": " __VA_ARGS__);}}while(0)

#if (gSerialDebugLevel > 0)
#define SERIAL_FUNCTION_IN(...) do{SERIAL_FUNCTION_PRINTFLF("\r\n\r\nFUNCTION [%s] IN " __VA_ARGS__);}while(0)
#define SERIAL_FUNCTION_OUT(...) do{SERIAL_FUNCTION_PRINTFLF("\r\nFUNCTION [%s] OUT " __VA_ARGS__);}while(0)
#define SERIAL_FUNCTION_LOGI(...) do{SERIAL_FUNCTION_PRINTFLF("\r\n\r\nI: FUNCTION [%s] " __VA_ARGS__);}while(0)
#define SERIAL_FUNCTION_TAG_LOGI(x, ...) do{SERIAL_FUNCTION_PRINTFLF("I " x ": FUNCTION [%s] " __VA_ARGS__);}while(0)
#else
#define SERIAL_FUNCTION_IN(...)
#define SERIAL_FUNCTION_OUT(...)
#define SERIAL_FUNCTION_LOGI(...)
#define SERIAL_FUNCTION_TAG_LOGI(x, ...)
#endif

#endif // __SERIAL_TRACE_H
