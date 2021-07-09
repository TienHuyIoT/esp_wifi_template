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
  if (port != 0)
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

void begin(uint16_t port = 0);

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

/* Includes ------------------------------------------------------------------*/
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

#if (ESP_IDF_VERSION_MAJOR >= 4) // IDF 4+
#include <aes/esp_aes.h>
#include <esp_sntp.h>
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include <esp32/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32S2
#include <esp32s2/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32C3
#include <esp32c3/rom/rtc.h>
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include <hwcrypto/aes.h>
#include <rom/rtc.h>
#include <lwip/apps/sntp.h>
#endif

#include <soc/rtc.h>
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
#include "log_report.h"
#include "fs_handle.h"
#include "sd_card.h"
#include "fs_editor.h"
#include "esp_reset.h"
#include "server_data_process.h"
#include "async_webserver.h"
#include "async_websocket.h"
#include "app_async_websocket.h"
#include "flatform_rtc.h"
#include "console_dbg.h"

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define MAIN_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[MAIN]", __VA_ARGS__)
#define MAIN_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)

/* Private variables ---------------------------------------------------------*/
Ticker led_ticker;

#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1)
DNSServer dnsServer;
#endif

hw_timer_t *timer = NULL;

#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
/* Object input factory reset */
IOInput input_factory_reset(FACTORY_INPUT_PIN,HIGH,10,10,10);
#endif

// inteval timeout check temperature
TimeOutEvent internal_temp_to(60000);

const char * build_time = "Thu " __DATE__ " " __TIME__ " GMT";

/* Private function prototypes -----------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif
 
uint8_t temprature_sens_read(void);
 
#ifdef __cplusplus
}
#endif
 
uint8_t temprature_sens_read(void);
void log_report(uint8_t log_id, char *p_log);
void sntp_setup(void);
void wifi_init(void);
void wifi_off(void);
int esp_ssid_scan(String &json);
void hw_wdt_init(uint32_t t_milisec);
void hw_wdt_feed(void);
void IRAM_ATTR hw_resetModule(void);
void print_reset_reason(RESET_REASON reason);
void verbose_print_reset_reason(RESET_REASON reason);
void reason_reset_log(void);
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
void wakeup_reason_log(void);
void wifi_events_setup(led_callback_t cb);
void wifi_setup(const char* name, const char* pass);
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
uint8_t eth_init(void);
void eth_enable(void);
void eth_disable(void);
uint8_t eth_is_enable(void);
void ETHGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
#endif
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEvent(WiFiEvent_t event);
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)
void ddns_client_init(void);
void ddns_update(void);
#endif
float esp32_internal_temp(void);
void internal_temp_log_report(void);
#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
uint8_t factory_reset_handle(void);
#endif

/* Private user code ---------------------------------------------------------*/
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

  CONSOLE_PORT.begin(115200, SERIAL_8N1, -1, 1);
  MAIN_TAG_CONSOLE("build_time: %s", build_time);
  MAIN_TAG_CONSOLE("==== Firmware version %u.%u.%u ====\r\n", 
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
  ArduinoOTA.onStart(
      []()
      {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
          type = "sketch";
        }
        else // U_SPIFFS
        {
          type = "filesystem";
          NAND_FS_SYSTEM.end();
        }

        /** NOTE: if updating SPIFFS this would be the place
         *  to unmount SPIFFS using SPIFFS.end() */
        MAIN_TAG_CONSOLE("Start updating %s", type.c_str());
      });

  ArduinoOTA.onEnd([]()
                   { MAIN_TAG_CONSOLE("End"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
                          // MAIN_TAG_CONSOLE("Progress: %u%%", (progress / (total / 100)));
                          /* Watch dog timer feed */
                          hw_wdt_feed();
                        });

  ArduinoOTA.onError([](ota_error_t error)
                     {
                       MAIN_TAG_CONSOLE("Error[%u]: ", error);
                       if (error == OTA_AUTH_ERROR)
                       {
                         MAIN_CONSOLE("Auth Failed");
                       }
                       else if (error == OTA_BEGIN_ERROR)
                       {
                         MAIN_CONSOLE("Begin Failed");
                       }
                       else if (error == OTA_CONNECT_ERROR)
                       {
                         MAIN_CONSOLE("Connect Failed");
                       }
                       else if (error == OTA_RECEIVE_ERROR)
                       {
                         MAIN_CONSOLE("Receive Failed");
                       }
                       else if (error == OTA_END_ERROR)
                       {
                         MAIN_CONSOLE("End Failed");
                       }
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