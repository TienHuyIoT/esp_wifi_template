#include "rtc_data_file.h"

#define RTC_PORT Serial
#define RTC_PRINTF(f_, ...) RTC_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void rtc_setup(void)
{
    struct tm tmstruct;
    // Set timezone to vietnam Standard Time
    setenv("TZ", "CST-7", 1);
    tzset();
    if (!getLocalTime(&tmstruct, 1))
    {
        rtc_time_t rtc = RTC_TIME_DEFAULT;
        if (rtc_info_read(&rtc))
        {
            rtc_info_remove();
        }
        // rtc.year = 2020;
        // rtc.mon = 8;
        // rtc.mday = 2;
        // rtc.hour = 22;
        // rtc.min = 51;
        // rtc.sec = 0;
        // rtc.wday = 0; // sunday(0), sun(0) ... sat(6)
        rtc_set(&rtc);
        rtc_get(&rtc);
        RTC_PRINTF("\r\nInit RTC default\r\n");        
    }    
}

uint8_t rtc_level_update_get(void)
{
    return rtc_level_update;
}

void rtc_level_update_set(uint8_t level)
{
    RTC_PRINTF("\r\nRtc level set: %u", level);
    if (level > rtc_level_update)
    {        
        RTC_PRINTF(" Access \r\n");
        rtc_level_update = level;
    }else
    {
        RTC_PRINTF(" Deny \r\n");
    }
}

void rtc_set_time(rtc_time_t *rtc)
{
    rtc_time_t g_rtc = RTC_TIME_DEFAULT;
    rtc_get(&g_rtc);
    g_rtc.hour = rtc->hour;
    g_rtc.min = rtc->min;
    g_rtc.sec = rtc->sec;
    rtc_set(&g_rtc);
}

void rtc_set_date(rtc_time_t *rtc)
{
    rtc_time_t g_rtc = RTC_TIME_DEFAULT;
    rtc_get(&g_rtc);
    g_rtc.year = rtc->year;
    g_rtc.mon = rtc->mon;
    g_rtc.mday = rtc->mday;        
    rtc_set(&g_rtc);
}

void rtc_set(rtc_time_t *rtc)
{
    struct timeval now;
    time_t false_now = 0;
    false_now = rtc_mktime(rtc);
    now = {.tv_sec = false_now};
    settimeofday(&now, NULL);
}

void rtc_localtime(rtc_time_t* rtc, uint32_t now)
{
    struct tm desired_tm;
    localtime_r((time_t *)&now, &desired_tm);
    rtc->mon = desired_tm.tm_mon + 1;
    rtc->mday = desired_tm.tm_mday;
    rtc->year = desired_tm.tm_year + 1900;
    rtc->hour = desired_tm.tm_hour;
    rtc->min = desired_tm.tm_min;
    rtc->sec = desired_tm.tm_sec;
    rtc->wday = desired_tm.tm_wday;
}

uint32_t rtc_mktime(rtc_time_t* rtc)
{
    struct tm desired_tm;
    time_t false_now = 0;
    memset(&desired_tm, 0, sizeof(struct tm));
    // Setting up a false actual time - used
    // when the file is created and for modification with the current time
    desired_tm.tm_mon = rtc->mon - 1;
    desired_tm.tm_mday = rtc->mday;
    desired_tm.tm_year = rtc->year - 1900;
    desired_tm.tm_hour = rtc->hour;
    desired_tm.tm_min = rtc->min;
    desired_tm.tm_sec = rtc->sec;
    desired_tm.tm_wday = rtc->wday;
    false_now = mktime(&desired_tm);
    return (uint32_t)false_now;
}

uint8_t rtc_get(rtc_time_t *rtc)
{
    struct tm tmstruct;
    if (getLocalTime(&tmstruct, 1))
    {
        rtc->hour = tmstruct.tm_hour;
        rtc->min = tmstruct.tm_min;
        rtc->sec = tmstruct.tm_sec;
        rtc->year = tmstruct.tm_year + 1900;
        rtc->mon = tmstruct.tm_mon + 1;
        rtc->mday = tmstruct.tm_mday;
        rtc->wday = tmstruct.tm_wday;
        RTC_PORT.println(&tmstruct, "\r\n%A, %B %d %Y %H:%M:%S");
        return true;
    }
    return false;
}