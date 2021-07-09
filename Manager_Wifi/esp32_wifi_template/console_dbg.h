#ifndef _CONSOLE_CONSOLE_H
#define _CONSOLE_CONSOLE_H

#include "Arduino.h"

#define CONSOLE_PORT Serial
#define DBG_PRINTF(f_, ...)           CONSOLE_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

#define g_debugLevel 4

#define CONSOLE_LOGE(...) {if(g_debugLevel >= 0) {DBG_PRINTF(__VA_ARGS__);}}
#define CONSOLE_LOGW(...) {if(g_debugLevel >= 1) {DBG_PRINTF(__VA_ARGS__);}}
#define CONSOLE_LOGI(...) {if(g_debugLevel >= 2) {DBG_PRINTF(__VA_ARGS__);}}
#define CONSOLE_LOGD(...) {if(g_debugLevel >= 3) {DBG_PRINTF(__VA_ARGS__);}}
#define CONSOLE_LOGV(...) {if(g_debugLevel >= 4) {DBG_PRINTF(__VA_ARGS__);}}

#define CONSOLE_TAG_LOGE(x, ...) {if(g_debugLevel >= 0) {DBG_PRINTF("E %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}
#define CONSOLE_TAG_LOGW(x, ...) {if(g_debugLevel >= 1) {DBG_PRINTF("W %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}
#define CONSOLE_TAG_LOGI(x, ...) {if(g_debugLevel >= 2) {DBG_PRINTF("I %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}
#define CONSOLE_TAG_LOGD(x, ...) {if(g_debugLevel >= 3) {DBG_PRINTF("D %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}
#define CONSOLE_TAG_LOGV(x, ...) {if(g_debugLevel >= 4) {DBG_PRINTF("V %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}

#endif
