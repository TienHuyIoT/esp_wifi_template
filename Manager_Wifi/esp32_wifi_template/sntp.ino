#include "rtc_data_file.h"

void sntp_setup()
{
    Serial.println("Contacting Time Server");
    sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    // while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {};
}

void sntp_sync_time_cb(struct timeval *tv)
{
    struct tm timeinfo;
    gettimeofday(tv, NULL);
    localtime_r(&tv->tv_sec, &timeinfo);
    Serial.println("\nSntp sync time");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

    rtc_level_update_set(RTC_SNTP_UPDATE);
}