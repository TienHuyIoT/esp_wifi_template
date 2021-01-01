#include <ArduinoJson.h>
#include "rtc_data_file.h"

#define RTC_FILE_PORT Serial
#define RTC_FILE_PRINTF(f_, ...) RTC_FILE_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

// rtc json string default
const char rtc_file_default[] PROGMEM =
    "{"
    "\n\"year\":1930,"
    "\n\"mon\":1,"
    "\n\"mday\":1,"
    "\n\"hour\":0,"
    "\n\"min\":0,"
    "\n\"sec\":0,"
    "\n\"wday\":3"
    "\n}";

void rtc_info_write(rtc_time_t *w_rtc_info)
{
    File fs_handle;
    DynamicJsonBuffer djbco;
    JsonObject &root = djbco.createObject();
    root["year"].set(w_rtc_info->year);
    root["mon"].set(w_rtc_info->mon);
    root["mday"].set(w_rtc_info->mday);
    root["hour"].set(w_rtc_info->hour);
    root["min"].set(w_rtc_info->min);
    root["sec"].set(w_rtc_info->sec);
    root["wday"].set(w_rtc_info->wday);

    RTC_FILE_PRINTF("\r\nJson created: ");
    root.printTo(RTC_FILE_PORT);
    fs_handle = NAND_FS_SYSTEM.open(RTC_FILE_PATH, FILE_WRITE);
    root.prettyPrintTo(fs_handle);
    fs_handle.close();
    RTC_FILE_PRINTF("\r\nrtc info updated");
}

uint8_t rtc_info_read(rtc_time_t *r_rtc_info)
{
    File fs_handle;
    // check file exist
    if (!NAND_FS_SYSTEM.exists(RTC_FILE_PATH))
    {
        // write json string default
        // fs_handle = NAND_FS_SYSTEM.open(RTC_FILE_PATH, FILE_WRITE);
        // fs_handle.printf_P(rtc_file_default);
        // fs_handle.close();
        return 0;
    }

    fs_handle = NAND_FS_SYSTEM.open(RTC_FILE_PATH, FILE_READ);

    DynamicJsonBuffer djbpo;
    JsonObject &root = djbpo.parseObject(fs_handle);
    fs_handle.close();

    if (!root.success())
    {
        RTC_FILE_PRINTF("\r\nJSON parsing failed!");
        return 0;
    }
    r_rtc_info->year = root["year"].as<int>();
    r_rtc_info->mon = root["mon"].as<int>();
    r_rtc_info->mday = root["mday"].as<int>();
    r_rtc_info->hour = root["hour"].as<int>();
    r_rtc_info->min = root["min"].as<int>();
    r_rtc_info->sec = root["sec"].as<int>();
    r_rtc_info->wday = root["wday"].as<int>();

    return 1;
}

void rtc_info_remove()
{
    if (NAND_FS_SYSTEM.remove(RTC_FILE_PATH))
    {
        RTC_FILE_PRINTF("\r\n- %s file deleted\r\n", RTC_FILE_PATH);
    }
    else
    {
        RTC_FILE_PRINTF("\r\n- %s delete failed\r\n", RTC_FILE_PATH);
    }
}

/* "Thu Jan 25 2018 19:39:48 GMT+0700 (SE Asia Standard Time)" */
uint8_t rtc_parse_utility(char *rtc_web)
{
    rtc_time_t rtc = {0};
    char wday[15], mon[15];
    uint8_t result = 0;
    uint8_t field;

    field = sscanf(rtc_web, "%3s %3s %u %u %u:%u:%u", 
            wday, mon, &rtc.mday, &rtc.year, &rtc.hour, &rtc.min, &rtc.sec);
    RTC_FILE_PRINTF("\r\nfield: [%u]: %s", field, rtc_web);
    if (7 == field)
    {
        const char *const mon_list[] PROGMEM = {
            "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };
        RTC_FILE_PRINTF("\r\n%s %s %u %u %u:%u:%u", 
            wday, mon, rtc.mday, rtc.year, rtc.hour, rtc.min, rtc.sec);
        
        for (rtc.mon = 1; rtc.mon <= 12; ++rtc.mon)
        {
            if(!strcmp_P(mon,PSTR(mon_list[rtc.mon])))
            {
                RTC_FILE_PRINTF("\r\nParse month: %u", rtc.mon);
                break;
            }
        }
        if (rtc_level_update_get() < (uint8_t)RTC_SNTP_UPDATE)
        {
            RTC_FILE_PRINTF("\r\ntime update");
            rtc_set(&rtc);
            rtc_get(&rtc);
            rtc_level_update_set(RTC_WEB_UPATE);
        }
        else
        {
            RTC_FILE_PRINTF("\r\ntime update low priority");
        }
        result = 1;      
    }

    return result;
}