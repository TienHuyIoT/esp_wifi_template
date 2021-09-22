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
#define DBG_PRINTF(f_, ...)           CONSOLE_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

#define g_debugLevel 4

#define CONSOLE_LOGE(...) do{if(g_debugLevel >= 0) {DBG_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGW(...) do{if(g_debugLevel >= 1) {DBG_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGI(...) do{if(g_debugLevel >= 2) {DBG_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGD(...) do{if(g_debugLevel >= 3) {DBG_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGV(...) do{if(g_debugLevel >= 4) {DBG_PRINTF(__VA_ARGS__);}}while(0)

#define CONSOLE_TAG_LOGE(x, ...) do{if(g_debugLevel >= 0) {DBG_PRINTF("E %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}while(0)
#define CONSOLE_TAG_LOGW(x, ...) do{if(g_debugLevel >= 1) {DBG_PRINTF("W %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}while(0)
#define CONSOLE_TAG_LOGI(x, ...) do{if(g_debugLevel >= 2) {DBG_PRINTF("I %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}while(0)
#define CONSOLE_TAG_LOGD(x, ...) do{if(g_debugLevel >= 3) {DBG_PRINTF("D %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}while(0)
#define CONSOLE_TAG_LOGV(x, ...) do{if(g_debugLevel >= 4) {DBG_PRINTF("V %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}while(0)

#if (g_debugLevel > 0)
#define FUNCTION_IN(...) do{DBG_PRINTF("\r\n\r\n\t[%s] IN ", __FUNCTION__); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}while(0)
#define FUNCTION_OUT(...) do{DBG_PRINTF("\r\n\t[%s] OUT ", __FUNCTION__); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n\r\n");}while(0)
#endif

#endif // __SERIAL_TRACE_H
