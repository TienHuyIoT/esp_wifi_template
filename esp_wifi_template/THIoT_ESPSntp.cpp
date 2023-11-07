#ifdef ESP32
#include <esp_idf_version.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiType.h>
#include <time.h>
#if ESP_IDF_VERSION_MAJOR >= 4
#include <esp_sntp.h>
#else
#include <lwip/apps/sntp.h>
#endif
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <user_interface.h>
#include <TZ.h>
#include <time.h>      // time() ctime()
#include <sys/time.h>  // struct timeval
#include <coredecls.h> // settimeofday_cb()
#include <sntp.h>      // sntp_servermode_dhcp()

#define MYTZ TZ_Asia_Ho_Chi_Minh

// OPTIONAL: change SNTP startup delay
// a weak function is already defined and returns 0 (RFC violation)
// it can be redefined:
#if (0)
uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000 ()
{
   //info_sntp_startup_delay_MS_rfc_not_less_than_60000_has_been_called = true;
   return 60000; // 60s (or lwIP's original default: (random() % 5000))
}
#endif

// OPTIONAL: change SNTP update delay
// a weak function is already defined and returns 1 hour
// it can be redefined:
#if (0)
uint32_t sntp_update_delay_MS_rfc_not_less_than_15000 ()
{
  //  info_sntp_update_delay_MS_rfc_not_less_than_15000_has_been_called = true;
   return 15000; // 15s
}
#endif
#endif

#include "THIoT_APPConfig.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPTimeType.h"
#include "THIoT_ESPSntp.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define SNTP_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[SNTP]", __VA_ARGS__)

#define SNTP_TAG_LOG(...) FS_TAG_LOGI("[SNTP]", __VA_ARGS__)

extern "C" void configTime(long gmtOffset_sec, int daylightOffset_sec, const char* server1, const char* server2, const char* server3);
extern "C" void configTzTime(const char* tz, const char* server1, const char* server2, const char* server3);

#ifdef ESP32
static void sntp_sync_time_cb(struct timeval *tv) {
    SNTP_TAG_CONSOLE("callback settimeofday(SNTP)");
#elif defined(ESP8266)
static void sntp_sync_time_cb(bool from_sntp /* <= this parameter is optional */) {
    SNTP_TAG_CONSOLE("callback settimeofday(%s)", from_sntp ? "SNTP" : "USER");
    // SNTP_TAG_LOG("callback settimeofday(%s)", from_sntp ? "SNTP" : "USER");
#endif 
    SNTP_TAG_CONSOLE("callback now = %lu", tv->tv_sec);
    SNTP_TAG_CONSOLE("callback time: %s", ESPTime.toString(tv->tv_sec).c_str());

    /* Don't write data to SD card from callback */
    // SNTP_TAG_LOG("tNow = %lu", tv->tv_sec);
    // SNTP_TAG_LOG("Time: %s", ESPTime.toString(tv->tv_sec).c_str());
    ESPTime.setSourceUpdate(ESPTimeSystem::ESP_RTC_SNTP_UPDATE);
}

ESPSntpService::ESPSntpService() {}
ESPSntpService::~ESPSntpService() {}
String ESPSntpService::_server1 = String();
String ESPSntpService::_server2 = String();
String ESPSntpService::_timeZone = String();
long ESPSntpService::_gtmOffset = 7 * 3600;
int ESPSntpService::_daylightOffset = 0;

void ESPSntpService::begin()
{
    _server1 = ESPConfig.server1SNTP();
    _server2 = ESPConfig.server2SNTP();
    _timeZone = ESPConfig.TzTime();
    _gtmOffset = ESPConfig.gmtOffsetSNTP() * 3600;
    _daylightOffset = ESPConfig.daylightOffsetSNTP();
    SNTP_TAG_CONSOLE("configure time server");
    SNTP_TAG_CONSOLE("gtmOffset : %ld", _gtmOffset);
    SNTP_TAG_CONSOLE("daylightOffset : %d", _daylightOffset);
    SNTP_TAG_CONSOLE("TzTime : %s", _timeZone.c_str());
    SNTP_TAG_CONSOLE("server1: %s", _server1.c_str());
    SNTP_TAG_CONSOLE("server2: %s", _server2.c_str());
#if (TIME_ZONE_TYPE_CFG == 1)
    SNTP_TAG_LOG("configTime(%ld, %d, %s, %s)"
                , _gtmOffset
                , _daylightOffset
                , _server1.c_str()
                , _server2.c_str());
#else
    SNTP_TAG_LOG("configTime(%s, %s, %s)"
                , _timeZone.c_str()
                , _server1.c_str()
                , _server2.c_str());
#endif
#ifdef ESP32
    sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
#if (TIME_ZONE_TYPE_CFG == 1)
    configTime(_gtmOffset, _daylightOffset, _server1.c_str(), _server2.c_str());
#else
    configTzTime(_timeZone.c_str(), _server1.c_str(), _server2.c_str());
#endif
    // Using callback event instead to check sntp status
    // while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {};
#elif defined(ESP8266)
    // install callback - called when settimeofday is called (by SNTP or user)
    // once enabled (by DHCP), SNTP is updated every hour by default
    // ** optional boolean in callback function is true when triggered by SNTP **
    settimeofday_cb(sntp_sync_time_cb);
    // OPTIONAL: disable obtaining SNTP servers from DHCP
    sntp_servermode_dhcp(0);    
#if (TIME_ZONE_TYPE_CFG == 1)
    configTime(_gtmOffset, _daylightOffset, _server1.c_str(), _server2.c_str());
#else
    configTzTime(_timeZone.c_str(), _server1.c_str(), _server2.c_str());
#endif

    // Give now a chance to the settimeofday callback,
    // because it is *always* deferred to the next yield()/loop()-call.
    yield();
#endif
}
