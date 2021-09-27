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
#define CONSOLE_PORT Serial
#define CONSOLE_BAUDRATE 921600
#define CONSOLE_FLUSH() (CONSOLE_PORT.flush())
#define DBG_PRINTF(f_, ...) CONSOLE_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)
#define DBG_PRINTFLF(f_, ...) CONSOLE_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)
#define DBG_TAG_PRINTF(tag, f_, ...)  CONSOLE_PORT.printf_P(PSTR(tag f_), ##__VA_ARGS__)
#define DBG_FUNCTION_PRINTFLF(f_, ...)  CONSOLE_PORT.printf_P(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)

#define g_debugLevel 4

#define CONSOLE_LOGE(...) do{if(g_debugLevel > 0) {DBG_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGW(...) do{if(g_debugLevel > 1) {DBG_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGI(...) do{if(g_debugLevel > 2) {DBG_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGD(...) do{if(g_debugLevel > 3) {DBG_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGV(...) do{if(g_debugLevel > 4) {DBG_PRINTF(__VA_ARGS__);}}while(0)

#define CONSOLE_TAG_LOGE(x, ...) do{if(g_debugLevel > 0) {DBG_PRINTFLF("E " x ": " __VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGW(x, ...) do{if(g_debugLevel > 1) {DBG_PRINTFLF("W " x ": " __VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGI(x, ...) do{if(g_debugLevel > 2) {DBG_PRINTFLF("I " x ": " __VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGD(x, ...) do{if(g_debugLevel > 3) {DBG_PRINTFLF("D " x ": " __VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGV(x, ...) do{if(g_debugLevel > 4) {DBG_PRINTFLF("V " x ": " __VA_ARGS__);}}while(0)

#if (g_debugLevel > 0)
#define FUNCTION_IN(...) do{DBG_FUNCTION_PRINTFLF("\r\n\r\nFUNCTION [%s] IN " __VA_ARGS__);}while(0)
#define FUNCTION_OUT(...) do{DBG_FUNCTION_PRINTFLF("\r\nFUNCTION [%s] OUT " __VA_ARGS__);}while(0)
#define FUNCTION_LOGI(...) do{DBG_FUNCTION_PRINTFLF("\r\n\r\nI: FUNCTION [%s] " __VA_ARGS__);}while(0)
#define FUNCTION_TAG_LOGI(x, ...) do{DBG_FUNCTION_PRINTFLF("I " x ": FUNCTION [%s] " __VA_ARGS__);}while(0)
#else
#define FUNCTION_IN(...)
#define FUNCTION_OUT(...)
#define FUNCTION_LOGI(...)
#define FUNCTION_TAG_LOGI(x, ...)
#endif

#endif // __SERIAL_TRACE_H
