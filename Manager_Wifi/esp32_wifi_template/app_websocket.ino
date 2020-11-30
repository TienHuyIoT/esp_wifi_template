#include <ArduinoJson.h>
#include "app_websocket.h"
#include "rtc_data_file.h"

#define APP_WS_DBG_PORT Serial
#define APP_WS_DBG_PRINT(fmt, ...) APP_WS_DBG_PORT.printf_P(PSTR("\r\n<APP_WS> " fmt), ##__VA_ARGS__)

void ws_receive_txt_callback(uint8_t ws_index, uint8_t * payload)
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
      if(s != '\0'){
        rtc_time_t Rtc = {0};
        char wday[15], mon[15];
        uint8_t result;

        result = sscanf(s, "%3s %3s %u %u %u:%u:%u", 
                wday, mon, &Rtc.mday, &Rtc.year, &Rtc.hour, &Rtc.min, &Rtc.sec);
        APP_WS_DBG_PRINT("result: %u", result);
        if (7 == result)
        {
            const char *const mon_list[] PROGMEM = {
                "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
            };
            APP_WS_DBG_PRINT("%s %s %u %u %u:%u:%u", 
                wday, mon, Rtc.mday, Rtc.year, Rtc.hour, Rtc.min, Rtc.sec);
            
            for (Rtc.mon = 1; Rtc.mon <= 12; ++Rtc.mon)
            {
                if(!strcmp_P(mon,PSTR(mon_list[Rtc.mon])))
                {
                    APP_WS_DBG_PRINT("Parse month: %u", Rtc.mon);
                    break;
                }
            }
            if (rtc_level_update_get() < (uint8_t)RTC_SNTP_UPDATE)
            {
                APP_WS_DBG_PRINT("ws time update");
                rtc_set(&Rtc);
                rtc_get(&Rtc);
                rtc_level_update_set(RTC_WEB_UPATE);
            }
            else
            {
                APP_WS_DBG_PRINT("ws time update low priority");
            }
            
        }        
      }
    }
}