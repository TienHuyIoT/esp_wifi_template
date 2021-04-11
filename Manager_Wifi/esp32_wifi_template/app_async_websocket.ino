#include <ArduinoJson.h>
#include "app_async_websocket.h"
#include "rtc_data_file.h"

#define APP_WS_DBG_PORT Serial
#define APP_WS_DBG_PRINT(fmt, ...) APP_WS_DBG_PORT.printf_P(PSTR("\r\n[APP_WS] " fmt), ##__VA_ARGS__)

void ws_receive_txt_callback(uint8_t ws_index, char * payload)
{
    uint8_t page;
    uint8_t cmd;  
    uint8_t result_cmd = 0;

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(payload);
    if (!root.success())
    {
        APP_WS_DBG_PRINT("JSON parsing failed!");
        return;
    }

    if (!root["page"].success() || !root["cmd"].success())
    {
        return;
    }

    page = root["page"];
    cmd  = root["cmd"];

    APP_WS_DBG_PRINT("Page: %s", PSTR(ws_page_list[page]));
    APP_WS_DBG_PRINT("cmd : %s", PSTR(page_card_user_list[cmd]));

    if (root["date"].success())
    {
      //"date":"Thu Jan 25 2018 19:39:48 GMT+0700 (SE Asia Standard Time)"
      const char *s = root["date"];
      if(s != nullptr){
        rtc_parse_utility(s, RTC_WEB_UPATE);   
      }
    }
}