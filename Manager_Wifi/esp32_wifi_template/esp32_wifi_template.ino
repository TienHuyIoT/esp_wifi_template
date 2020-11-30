#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <SPIFFS.h>
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
#include <Update.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <TimeOutEvent.h>
#include <IOInput.h>
#include "Tools.h"
#include "rtc_data_file.h"
#include "wifi_data_file.h"
#include "eeprom_data.h"
#include "app_config.h"
#include "board.h"
#include "sd_card.h"
#include "WebSocket.h"
#include "app_websocket.h"

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#include <FS.h>
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1)
#include <SD.h>
#include <SPI.h>
#else
#include <SD_MMC.h>
#endif
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

/* =========================================================
 * Web server 
 * =========================================================*/
WebServer server(25123);
WebServer server80(80);
WebSocketsServer webSocket = WebSocketsServer(25124);

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
 * handle file system
 * =========================================================*/
File fs_handle;
File fsUploadFile;
File sd_uploadfile;

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
  COMMON_PORT.begin(115200);
  COMMON_PRINTF("\r\n==== Firmware version %u.%u.%u ====\r\n", 
                FW_VERSION_MAJOR,
                FW_VERSION_MINOR,
                FW_VERSION_BUILD);

  /* Init nand memory file system */
  NAND_FS_SYSTEM.begin();
  
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

  /* Init watch dog timer system 120s*/
  hw_wdt_init(120000);

  /* Update log */
  log_report(LOG_REPORT_INIT, (char*)"Board Initialize");

  /* Update log reset reason */
  reason_reset_log();

  /* Update log wakeup reason */
  wakeup_reason_log();

  /* Init wifi and web server */
  wifi_info_setup();
  wifi_events_setup();
  web_server_setup();
  wifi_init();
  web_server_init();
  web_socket_init(&ws_receive_txt_callback);
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

#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
  /* factory reset input handle */
  factory_reset_handle();
#endif

  /* Webserver handler */
  server.handleClient();
  server80.handleClient();

  /* Ws handle */
  webSocket.loop();
  ws_interval_sync();
}