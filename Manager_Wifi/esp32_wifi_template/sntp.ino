#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
#include <esp_sntp.h>
#else
#include <lwip/apps/sntp.h>
#endif
#include "rtc_data_file.h"
#include "console_dbg.h"

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 3600 * 7;
const int   daylightOffset_sec = 0;

extern void configTime(long gmtOffset_sec, int daylightOffset_sec, const char* server1, const char* server2, const char* server3);

void sntp_setup()
{
    CONSOLE_PORT.println("\r\nContacting Time Server");
    sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    // Using callback event instead to check sntp status
    // while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {};
}

void sntp_sync_time_cb(struct timeval *tv)
{
    struct tm timeinfo;
    gettimeofday(tv, NULL);
    localtime_r(&tv->tv_sec, &timeinfo);
    CONSOLE_PORT.println("\r\nSntp sync time");
    CONSOLE_PORT.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

    rtc_level_update_set(RTC_SNTP_UPDATE);
}