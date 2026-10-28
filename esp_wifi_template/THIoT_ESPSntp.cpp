#ifdef ESP32
#include <esp_idf_version.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiType.h>
#include <time.h>
#include <lwip/tcpip.h>
#include "freertos/FreeRTOS.h"
#include <esp_sntp.h>
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
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPResetReason.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define SNTP_TAG_CONSOLE(...)   CONSOLE_TAG_LOGI("[SNTP]", __VA_ARGS__)

#define SNTP_TAG_LOG(...)       FS_TAG_LOGI("[SNTP]", __VA_ARGS__)

#if (defined CONFIG_LWIP_SNTP_UPDATE_DELAY) && (CONFIG_LWIP_SNTP_UPDATE_DELAY > 3600000)
/**
 * If IDF configuration the sntp delay exceed than 60'. SNTP retry timeout should be 0 to retring update sntp is accurate to 1hour.
*/
#define SNTP_RETRY_TIMEOUT          (uint32_t)(60 * 5)                      /* Second */
#define SNTP_INTERVAL_TIMEOUT       3600U                                   /* Second */
#else
#define SNTP_RETRY_TIMEOUT          (uint32_t)(60 * 5)                      /* Second */
#define SNTP_INTERVAL_TIMEOUT       (uint32_t)(3600 + SNTP_RETRY_TIMEOUT)   /* Second */
#endif
#define SNTP_INTERVAL_TIMEOUT_1ST   60U                                     /* Second */

#define SNTP_RETRY_TIMES            3U
#define SNTP_RETRY_TIMES_MAX        10U

extern "C" void configTime(long gmtOffset_sec, int daylightOffset_sec, const char* server1, const char* server2, const char* server3);
extern "C" void configTzTime(const char* tz, const char* server1, const char* server2, const char* server3);
RTC_NOINIT_ATTR int noInitSntpCount;

#ifdef ESP32
static void sntp_sync_time_cb(struct timeval *tv)
#elif defined(ESP8266)
static void sntp_sync_time_cb(bool from_sntp /* <= this parameter is optional */)
#endif 
{
    SNTP_TAG_CONSOLE("callback settimeofday(SNTP)");
    
#ifdef ESP32
    SNTP_TAG_CONSOLE("tNow = %lu", tv->tv_sec);
    SNTP_TAG_CONSOLE("Time: %s", ESPTime.toString(tv->tv_sec).c_str());
#elif defined(ESP8266)
    SNTP_TAG_CONSOLE("callback settimeofday(%s)", from_sntp ? "SNTP" : "USER");
    SNTP_TAG_LOG("callback settimeofday(%s)", from_sntp ? "SNTP" : "USER");
#endif
    
    ticker_once_ms(&ESPSntpService::_event, 100, [](void *arg){
        ESPSntpService::_syncEvent = true;
        ESPTime.setSourceUpdate(ESPTimeSystem::ESP_RTC_SNTP_UPDATE);

        SNTP_TAG_CONSOLE("Start interval update after %u(s)", SNTP_INTERVAL_TIMEOUT);
        ticker_attach_trigger(&ESPSntpService::_interval, SNTP_INTERVAL_TIMEOUT);
        noInitSntpCount = 0;
    }, nullptr);
}

ESPSntpService::ESPSntpService() {}
ESPSntpService::~ESPSntpService() {}
String ESPSntpService::_server1 = String();
String ESPSntpService::_server2 = String();
String ESPSntpService::_timeZone = String();
long ESPSntpService::_gtmOffset = 7 * 3600;
int ESPSntpService::_daylightOffset = 0;
boolean ESPSntpService::_syncEvent = false;
ticker_function_handle_t ESPSntpService::_interval = {.ticker =TICKER_STOP_INIT};
ticker_function_handle_t ESPSntpService::_event = {.ticker =TICKER_STOP_INIT};

void ESPSntpService::begin()
{
    if (esp_reset_reason_core(PRO_CPU0_ID) == POWERON_RESET) {
        noInitSntpCount = 0;
    }
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
    SNTP_TAG_LOG("configTime(%ld, %d, %s, %s) noInitSntpCount=%u"
                , _gtmOffset
                , _daylightOffset
                , _server1.c_str()
                , _server2.c_str()
                , noInitSntpCount);
#else
    SNTP_TAG_LOG("configTime(%s, %s, %s) noInitSntpCount=%u"
                , _timeZone.c_str()
                , _server1.c_str()
                , _server2.c_str()
                , noInitSntpCount);
#endif
#ifdef ESP32
    sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
#if (TIME_ZONE_TYPE_CFG == 1)
    configTime(_gtmOffset, _daylightOffset, _server1.c_str(), _server2.c_str());
#else
    configTzTime(_timeZone.c_str(), _server1.c_str(), _server2.c_str());
#endif
    SNTP_TAG_CONSOLE("Sync mode: %s", sntp_get_sync_mode() ? "SMOOTH" : "IMMED");
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

    SNTP_TAG_CONSOLE("Start interval retry after %u(s)", SNTP_RETRY_TIMEOUT);
    ticker_attach(&_interval, SNTP_RETRY_TIMEOUT, [](void *arg){
        ESPSntpService *handler = (ESPSntpService*)arg;
        if(ESPWifi.IsConnected() || Ethernet.IsConnected()) {
            handler->syncRestart();
        }
    }, this);
    ticker_attach_trigger(&_interval, SNTP_INTERVAL_TIMEOUT_1ST);
}

#if (ESP_IDF_VERSION_MAJOR < 4)
static void sntp_do_restart(void *ctx)
{
    (void)ctx;
    if (sntp_enabled()) {
        SNTP_TAG_CONSOLE("Stop");
        sntp_stop();
    }
    SNTP_TAG_CONSOLE("init");
    sntp_init();
}
#endif

bool ESPSntpService::syncRestart() {
    ++noInitSntpCount;
    if ((noInitSntpCount <= SNTP_RETRY_TIMES_MAX) && (noInitSntpCount % SNTP_RETRY_TIMES) == 0) {
        SNTP_TAG_LOG("Reset due to noInitSntpCount = %u", noInitSntpCount);
        SOFTReset.enable(500, ESPSoftReset::SNTP_TIMEOUT_TYPE);
        ticker_detach(&_interval);
        return false;
    }

#if (ESP_IDF_VERSION_MAJOR >= 4)
    if (esp_sntp_enabled()) {
        SNTP_TAG_LOG("Restart times %u", noInitSntpCount);
        sntp_restart();
    } else {
        esp_sntp_init();
    }
#else
    SNTP_TAG_LOG("Restart times %u", noInitSntpCount);
    tcpip_callback(sntp_do_restart, NULL);
#endif
    return true;
}
