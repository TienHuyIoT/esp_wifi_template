//==========================================================
// <o> FW_VERSION_MAJOR
 
#ifndef FW_VERSION_MAJOR
#define FW_VERSION_MAJOR 1
#endif

//==========================================================
// <o> FW_VERSION_MINOR
 
#ifndef FW_VERSION_MINOR
#define FW_VERSION_MINOR 0
#endif

//==========================================================
// <o> FW_VERSION_BUILD
 
#ifndef FW_VERSION_BUILD
#define FW_VERSION_BUILD 0
#endif

//==========================================================
// Macro debug
//==========================================================
#define COMMON_PORT Serial
#define COMMON_PRINTF(f_, ...) COMMON_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

// include the library code:
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <FS.h>
#include <SPIFFS.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <driver/rtc_io.h>
#include <hwcrypto/aes.h>
#include <rom/rtc.h>
#include <soc/rtc.h>
#include <lwip/apps/sntp.h>
#include <time.h>
#include <Update.h>
#include <WebServer.h>
#include <TimeOutEvent.h>
#include "Tools.h"
#include "rtc_data_file.h"
#include "wifi_data_file.h"
#include "eeprom_data.h"

hw_timer_t *timer = NULL;

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 3600 * 7;
const int   daylightOffset_sec = 0;

/* =========================================================
 * Web server 
 * =========================================================*/
WebServer server(25123);
WebServer server80(80);

/* 
*  https://circuits4you.com
*  ESP32 Internal Temperature Sensor Example
*/

#ifdef __cplusplus
extern "C" {
#endif
 
uint8_t temprature_sens_read();
 
#ifdef __cplusplus
}
#endif
 
uint8_t temprature_sens_read();

/* =========================================================
 * RTC 
 * =========================================================*/
typedef struct rtc_time{
  int    year;   /* 2020 */
  int    mon;    /* 1..12 */
  int    mday;    /* 1.. 31 */
  int    wday;    /* 0..6 */
  int    hour;    /* 0..23 */
  int    min;    /* 0..59 */
  int    sec;    /* 0..59 */
} rtc_time_t;

#define RTC_TIME_DEFAULT {1930, 1, 1, 3, 0, 0, 0}

/* =========================================================
 * handle file system
 * =========================================================*/

#define FILE_SYSTEM SPIFFS
File fs_handle;
File fsUploadFile;

#define LOG_REPORT_SIZE_BYTE      (1024 * 200)
#define LOG_REPORT_INIT           0
#define LOG_REPORT_SLEEP          1
#define LOG_REPORT_RESET          2
#define LOG_REPORT_WIFI           3
#define LOG_REPORT_TEMP           4
#define LOG_REPORT_OVER_TEMP      5
#define LOG_REPORT_PATH           "/log_report.csv"

// inteval timeout check temperature
TimeOutEvent internal_temp_to(60000);

void setup()
{
  COMMON_PORT.begin(115200);

  FILE_SYSTEM.begin();  // init spiffs  
  
  rtc_init(); // rtc init

  listDir(FILE_SYSTEM, "/", 0);

  eeprom_setup();
  
  COMMON_PRINTF("Hello my FANs!");  

  hw_wdt_init(120000);  // init wdt 120s  

  log_report(LOG_REPORT_INIT, (char*)"Board Init");

  reason_reset_log(); //log report reason reset

  wakeup_reason_log();  // log report reason wakeup

  wifi_info_setup();
  wifi_events_setup();
  web_server_setup();

  wifi_init();
  web_server_begin();
}

void loop()
{
  /* Watch dog timer feed */
  hw_wdt_feed();

  /* Run any function special */
  if (device_is_active())
  {
    
  }
  
  /* Monitor internal temperature */
  internal_temp_log_report();

  /* Webserver handler */
  server.handleClient();
  server80.handleClient();
}
