#ifdef ESP32
#include <time.h>
#if ESP_IDF_VERSION_MAJOR >= 4
#include <esp_sntp.h>
#else
#include <lwip/apps/sntp.h>
#endif
#elif defined(ESP8266)
#include <TZ.h>
#include <time.h>      // time() ctime()
#include <sys/time.h>  // struct timeval
#include <coredecls.h> // settimeofday_cb()
#include <sntp.h>      // sntp_servermode_dhcp()

#define MYTZ TZ_Asia_Ho_Chi_Minh
#endif

#include "hth_esp_sys_rtc.h"
#include "hth_console_dbg.h"
#include "hth_esp_sntp.hpp"

#define SNTP_CONSOLE_PORT CONSOLE_PORT
#define SNTP_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)
#define SNTP_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[SNTP]", __VA_ARGS__)

#ifdef ESP32
static void sntp_sync_time_cb(struct timeval *tv) {
    SNTP_TAG_CONSOLE("settimeofday(SNTP)");
#elif defined(ESP8266)
static void sntp_sync_time_cb(bool from_sntp /* <= this parameter is optional */) {
    SNTP_TAG_CONSOLE("settimeofday(%s)", from_sntp ? "SNTP" : "USER");
#endif 
    time_t now = time(nullptr);
    const tm* tm = localtime(&now);
    char buf[64];
    /** The same way using with esp32 
     * RTC_CONSOLE_PORT.println(&tmStruct, "\r\nTime: %A, %B %d %Y %H:%M:%S");
     * */
    strftime(buf, 64, "%A, %B %d %Y %H:%M:%S", tm);
    SNTP_TAG_CONSOLE("Time: %s", buf);

    HTH_sysTime.setSourceUpdate(flatform_rtc::RTC_SNTP_UPDATE);
}

hth_esp_sntp::hth_esp_sntp(/* args */)
{
}

hth_esp_sntp::~hth_esp_sntp()
{
}

// static constexpr long hth_esp_sntp::gmtOffset_sec = 3600 * 7;

void hth_esp_sntp::begin()
{
    SNTP_TAG_CONSOLE("Configure Time Server");
#ifdef ESP32
    sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
    configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer1, _ntpServer2);
    // Using callback event instead to check sntp status
    // while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {};
#elif defined(ESP8266)
    // install callback - called when settimeofday is called (by SNTP or user)
    // once enabled (by DHCP), SNTP is updated every hour by default
    // ** optional boolean in callback function is true when triggered by SNTP **
    settimeofday_cb(sntp_sync_time_cb);
    configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer1, _ntpServer2);
#endif
}
