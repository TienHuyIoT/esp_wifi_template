#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <NetBIOS.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <driver/rtc_io.h>
#include <hwcrypto/aes.h>
#include <rom/rtc.h>
#include <soc/rtc.h>
#include <lwip/apps/sntp.h>
#include <time.h>
#include <TimeOutEvent.h>
#include <IOInput.h>
#include <IOBlink.h>
#include "app_config.h"
#include "Tools.h"
#include "rtc_data_file.h"
#include "wifi_data_file.h"
#include "eeprom_data.h"
#include "board.h"
#include "sd_card.h"
#include "fs_editor.h"
#include "server_data_process.h"
#include "async_webserver.h"
#include "async_websocket.h"
#include "app_async_websocket.h"

#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1)
DNSServer dnsServer;
#endif

#define COMMON_PORT Serial
#define COMMON_PRINTF(f_, ...) COMMON_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

hw_timer_t *timer = NULL;

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 3600 * 7;
const int   daylightOffset_sec = 0;

#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
/* Object input factory reset */
IOInput input_factory_reset(FACTORY_INPUT_PIN,HIGH,10,10,10);
#endif

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

#define LOG_REPORT_SIZE_BYTE      (1024 * 200)
#define LOG_REPORT_INIT           0
#define LOG_REPORT_SLEEP          1
#define LOG_REPORT_RESET          2
#define LOG_REPORT_WIFI           3
#define LOG_REPORT_TEMP           4
#define LOG_REPORT_OVER_TEMP      5
#define LOG_REPORT_SD             6
#define LOG_REPORT_PATH           "/log_report.csv"

// inteval timeout check temperature
TimeOutEvent internal_temp_to(60000);

void setup()
{
  wifi_file_json_t *g_wifi_cfg;

  COMMON_PORT.begin(115200);
  COMMON_PRINTF("\r\n==== Firmware version %u.%u.%u ====\r\n", 
                FW_VERSION_MAJOR,
                FW_VERSION_MINOR,
                FW_VERSION_BUILD);   

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
#if (defined ETH_GPIO_ENABLE) && (ETH_GPIO_ENABLE != -1)
  ETH_GPIO_ENABLE_INIT();
  if(ETH_STATUS_IS_ON())
  {
    eth_enable();
  }
  else
  {
    eth_disable();
  }
#endif
#endif

  /* Init nand memory file system */
  NAND_FS_SYSTEM.begin(true);
  
  /* Init rtc of system */
  rtc_setup();

  /* List file in nand memory file system */
  listDir(NAND_FS_SYSTEM, "/", 0);

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  /* Init sd card */
  sd_card_init();
  /* List file in sd card memory file system */
  listDir(SD_FS_SYSTEM, "/", 0);
#endif  

  /* Init eeprom system */
  eeprom_setup();  

  /* Init watch dog timers*/
  hw_wdt_init(WDT_TIMEOUT_VALUE);

  /* Update log */
  log_report(LOG_REPORT_INIT, (char*)"Board Initialize");

  /* Update log reset reason */
  reason_reset_log();

  /* Update log wakeup reason */
  wakeup_reason_log();
  
  /* Init wifi */    
  wifi_events_setup();
  wifi_info_setup();
  g_wifi_cfg = wifi_info_get();

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
  if(eth_is_enable())
  {
    eth_init();
  }
  else
  {
    wifi_init();
  }
#else
  wifi_init();
#endif  

#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)
  //Send OTA events to the browser
  ArduinoOTA.onStart([]() { events.send("Update Start", "ota"); });
  ArduinoOTA.onEnd([]() { events.send("Update End", "ota"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress/(total/100)));
    events.send(p, "ota");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if(error == OTA_AUTH_ERROR) events.send("Auth Failed", "ota");
    else if(error == OTA_BEGIN_ERROR) events.send("Begin Failed", "ota");
    else if(error == OTA_CONNECT_ERROR) events.send("Connect Failed", "ota");
    else if(error == OTA_RECEIVE_ERROR) events.send("Recieve Failed", "ota");
    else if(error == OTA_END_ERROR) events.send("End Failed", "ota");
  });
    
  ArduinoOTA.setHostname(g_wifi_cfg->sta.HostName);
  ArduinoOTA.setPassword("1234");
  ArduinoOTA.begin();
#endif

#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1)
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(53, g_wifi_cfg->ap.DnsName, WiFi.softAPIP());  
#endif  

  /* Init web server */    
  web_server_setup();  
  web_server_init();
  web_socket_init(&ws_receive_txt_callback);
}

void loop()
{
  /* Restart esp handle */
  esp_reboot_handle();
  
  /* Watch dog timer feed */
  hw_wdt_feed();

  /* Run any function special */
  if (eeprom_device_is_activated())
  {
    
  }
  
  /* Monitor internal temperature */
  internal_temp_log_report();

#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
  /* factory reset input handle */
  factory_reset_handle();
#endif

  /* Ws handle */
  ws.cleanupClients();
  ws_interval_sync();

#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)  
  ArduinoOTA.handle();
#endif

#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1)  
  dnsServer.processNextRequest();
#endif
}