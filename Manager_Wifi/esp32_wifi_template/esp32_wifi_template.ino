/*
Edit lib async:

WebHandlerlmpl.h edit
line 32 add: typedef std::function<bool(AsyncWebServerRequest *request)> ArRequestAuthenticateFunction;
line 49 add: ArRequestAuthenticateFunction _onAuthenticate;
line 62 add: AsyncStaticWebHandler& onAuthenticate(ArRequestAuthenticateFunction fn) {_onAuthenticate = fn; return *this;}

WebHandlers.cpp edit
Line 193
if(_username != "" && _password != "")
{
  if(!request->authenticate(_username.c_str(), _password.c_str()))
  {
    return request->requestAuthentication();
  } 
}     
else
{
  if(_onAuthenticate)
  {
    if(!_onAuthenticate(request))
    {
      return;
    }
  }
}

******Fix Memory heap not free when using NBNS ******

AsyncUDP.cpp edit
Line 676:
void AsyncUDP::_recv(udp_pcb *upcb, pbuf *pb, const ip_addr_t *addr, uint16_t port, struct netif * netif)
{
  while(pb != NULL) {
      pbuf * this_pb = pb;
      pb = pb->next;
      this_pb->next = NULL;
      if(_handler) {
          AsyncUDPPacket packet(this, this_pb, addr, port, netif);
          _handler(packet);
      } else {
          pbuf_free(this_pb);
      }
  }
}

--> change to

void AsyncUDP::_recv(udp_pcb *upcb, pbuf *pb, const ip_addr_t *addr, uint16_t port, struct netif * netif)
{
  while(pb != NULL) {
      pbuf * this_pb = pb;
      pb = pb->next;
      this_pb->next = NULL;
      if(_handler) {
          AsyncUDPPacket packet(this, this_pb, addr, port, netif);
          _handler(packet);
      }
      
      pbuf_free(this_pb);
  }
}

***************Enable change port***************

1. WebServer.cpp at line  82
Original
void AsyncWebServer::begin(){
  _server.setNoDelay(true);  
  _server.begin();
}

Change to

void AsyncWebServer::begin(uint16_t port){
  if (port != NULL)
  {
    _server.port(port);
  }
  _server.setNoDelay(true);  
  _server.begin();
}

2. ESPAsyncWebServer.h at line 408
Original
void begin();

Change to

void begin(uint16_t port = NULL);

3. AsyncTCP.cpp at line 1264
Original
void AsyncServer::begin()
{}

Change to

void AsyncServer::port(uint16_t port)
{
    _port = port;
}

void AsyncServer::begin()
{}

4. AsyncTCP.h at line 194
Original
void begin();

Change to

void port(uint16_t port);
void begin();
*/

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
#include <Ticker.h>
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

Ticker led_ticker;

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

void led_status_blink_update(int time_bl)
{
  static int time_bl_backup = 0;
  if(time_bl_backup != time_bl)
  {
    time_bl_backup = time_bl;
    led_ticker.attach_ms(time_bl, led_status_toggle);
    led_status_on();
  }  
}

void setup()
{
  wifi_file_json_t *g_wifi_cfg;

  COMMON_PORT.begin(115200, SERIAL_8N1, -1, 1);
  COMMON_PRINTF("\r\nbuild_time: %s", build_time);
  COMMON_PRINTF("\r\n==== Firmware version %u.%u.%u ====\r\n", 
                FW_VERSION_MAJOR,
                FW_VERSION_MINOR,
                FW_VERSION_BUILD); 

  led_status_init();

  led_status_blink_update(1000);                  

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
  wifi_events_setup(led_status_blink_update);
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
    
  ArduinoOTA.setHostname(g_wifi_cfg->sta.hostname);
  ArduinoOTA.setPassword("1234");
  ArduinoOTA.begin();
#endif

#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1)
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(53, g_wifi_cfg->ap.dns_name, WiFi.softAPIP());  
#endif  

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)
  ddns_client_init();
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
  if(factory_reset_handle())
  {
    led_status_blink_update(3000);
  }
#endif

#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)  
  ArduinoOTA.handle();
#endif

#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1)  
  dnsServer.processNextRequest();
#endif
}