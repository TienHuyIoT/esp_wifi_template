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

void rtc_info_read(rtc_time_t *r_rtc_info)
{
    // check file exist
    if (!NAND_FS_SYSTEM.exists(RTC_FILE_PATH))
    {
        // write json string default
        fs_handle = NAND_FS_SYSTEM.open(RTC_FILE_PATH, FILE_WRITE);
        fs_handle.printf_P(rtc_file_default);
        fs_handle.close();
    }

    fs_handle = NAND_FS_SYSTEM.open(RTC_FILE_PATH, FILE_READ);
    size_t size = fs_handle.size();
    std::unique_ptr<char[]> buff(new char[size + 1]);
    fs_handle.readBytes(buff.get(), size);
    buff.get()[size] = 0;
    fs_handle.close();
    RTC_FILE_PRINTF("\r\nrtc file content: %s", buff.get());

    DynamicJsonBuffer djbpo(size + 100);
    JsonObject &root = djbpo.parseObject(buff.get());
    if (!root.success())
    {
        RTC_FILE_PRINTF("\r\nJSON parsing failed!");
        return;
    }
    r_rtc_info->year = root["year"].as<int>();
    r_rtc_info->mon = root["mon"].as<int>();
    r_rtc_info->mday = root["mday"].as<int>();
    r_rtc_info->hour = root["hour"].as<int>();
    r_rtc_info->min = root["min"].as<int>();
    r_rtc_info->sec = root["sec"].as<int>();
    r_rtc_info->wday = root["wday"].as<int>();
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
