#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <Hash.h>
#include <WiFiUdp.h>

#elif defined(ESP32)

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include "esp_wifi.h"
#endif

#include <WiFiClient.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <EEPROM.h>
#include <TimeOutEvent.h>
#include <IOBlink.h>
#include <IOInput.h>
#include "FileConfig.h"
#include "WifiConfig.h"

#ifdef ESP8266
#define COM_PORT Serial
#elif defined(ESP32)
#define COM_PORT Serial
#endif
#define COM_DEBUG(fmt, ...) COM_PORT.printf_P(PSTR("\r\n" fmt) ,##__VA_ARGS__)

/*=================================================================
  Khởi động Timeout Reboot ESP
  =================================================================*/
TimeOutEvent ESPRebootTo(0);

/*=================================================================
  Khởi tạo Object Blink IO cho Led Button
  =================================================================*/
#ifdef ESP8266
#define _LED_PIN  16
#define _BUZZ_PIN 5
#elif defined(ESP32)
#define _LED_PIN  22
#define _BUZZ_PIN 25
#endif
IOBlink LED_STT(_LED_PIN, HIGH);
IOBlink BUZZ_STT(_BUZZ_PIN, HIGH);

/*=================================================================
  Khởi tạo Webserver, Websocket Object
  =================================================================*/
#ifdef ESP8266
  ESP8266WebServer server(25123);
  ESP8266WebServer server80(80);
#elif defined(ESP32)
  WebServer server(25123);
  WebServer server80(80);
#endif
WebSocketsServer webSocket = WebSocketsServer(25124);

void Init_Setup(void){
  /*=================================================================
     - Khởi tạo Serial
     - Load cau hinh tu FS
    =================================================================*/
  COM_PORT.begin(115200);

  /* Khoi tao wifi de doc cac thong so wifi */
  #ifdef ESP8266
  //WiFi.setPhyMode(WIFI_PHY_MODE_11B);
  //WiFi.setOutputPower(20.5);
  #elif defined(ESP32)
  /*
  typedef enum {
    ESP_IF_WIFI_STA = 0,
    ESP_IF_WIFI_AP,     
    ESP_IF_ETH,         
    ESP_IF_MAX
  } esp_interface_t;
  */
  //esp_wifi_set_protocol(ESP_IF_WIFI_AP, WIFI_PROTOCOL_11B);
  //WiFi.setTxPower(WIFI_POWER_19_5dBm);
  COM_DEBUG("This Task runs on Core: %u",xPortGetCoreID());
  #endif  
  // WiFi.mode(WIFI_STA);  
  // WiFi.disconnect();
  
  COM_DEBUG("Hello ESP8266 Fans!");  
  COM_DEBUG("HwVersion: %s",HwVersion);
  COM_DEBUG("FwVersion: %s",FwVersion);
  SPIFFS.begin();
#ifdef ESP8266
  FileConfig.ListDir();
#elif defined(ESP32)
  FileConfig.ListDir(SPIFFS, "/", 0); /*List tất cả các file có trong bộ nhớ SPIFFS*/
#endif
  FileConfig.FS_FileConfig(Df_ReadConfig); //Load thong so tu FS
  FileConfig.EepromConfig(Df_ReadEep);
  FileConfig.ShowInfo();  

  /*=================================================================
    Khởi tạo timer led blink
  =================================================================*/  
  LED_STT.attach_ms(1, 100, 100);    //Khoi tao timer blink led
  BUZZ_STT.attach_ms(1, 100, 100);    //Khoi tao timer blink BUZZ  

  /*=================================================================
     Khởi tạo wifi
     - Hàm này được khởi tạo tùy thuộc trạng thái input trong Task_Config_Button_Proc()
    =================================================================*/
  WifiConfig.WifiLedStatus(&LED_STT);
  WifiConfig.WifiBuzzStatus(&BUZZ_STT);
  WifiConfig.WifiInit();  
  /*=================================================================
     Khởi tạo Hostname local
     Mo trinh duyet va go HostName.local/ để truy cập vào server
     New Lib 2.4.0 không phân biêt chữ hoa và chữ thường
    =================================================================*/
  if (!MDNS.begin(FileConfig.ConfigFile.STA.HostName))
  {
    COM_DEBUG("Error setting up MDNS responder!");
  }
  else
  {
    COM_DEBUG("mDNS responder started");
    // Add service to MDNS
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("http", "tcp", FileConfig.ConfigFile.TCPPort);
    MDNS.addService("ws", "tcp", FileConfig.ConfigFile.WSPort);
  }
  /*=================================================================
     Khởi tạo webserver và websocket
    =================================================================*/
  webSocket.begin();
  /*
    Load New Port. Gọi hàm này sau khi begin()
    Add in WebSocketsServer.cpp
    void WebSocketsServer::NewPort(int port) {
        _server->begin(port);
    }
    Add in WebSocketsServer.h
    void NewPort(int port = 80);
    */
  if (FileConfig.ConfigFile.TCPPort <= 80)
    FileConfig.ConfigFile.TCPPort = 25123;
  if (FileConfig.ConfigFile.WSPort <= 80)
    FileConfig.ConfigFile.WSPort = 25124;
  if (FileConfig.ConfigFile.WSPort == FileConfig.ConfigFile.TCPPort)
    FileConfig.ConfigFile.WSPort++;
  if (FileConfig.ConfigFile.WSPort != 25124)
    webSocket.NewPort(FileConfig.ConfigFile.WSPort);
  COM_DEBUG("Init Web Socket Port: %u", FileConfig.ConfigFile.WSPort);
  webSocket.onEvent(webSocketEvent);
  WebServerInit();
  WebUpdateInit();
  server.begin(FileConfig.ConfigFile.TCPPort);
  /*
    Load New Port. Gọi hàm này sau khi begin()
    Add in ESP8266WebServer.cpp
    void ESP8266WebServer::NewPort(int port) {
        _server.begin(port);
    }
    Add in ESP8266WebServer.h
    virtual void NewPort(int port = 80);
    server.NewPort(FileConfig.ConfigFile.TCPPort);
    */
  COM_DEBUG("Init Web Server Port: %u", FileConfig.ConfigFile.TCPPort);
  /*=================================================================
    Khi truy cap bang port 80 thi tu forward den port TCP da cai dat
    =================================================================*/
  server80.onNotFound([]() {
    server80.sendHeader("Location", String("http://") + server80.client().localIP().toString() + ":" + String(FileConfig.ConfigFile.TCPPort) + server80.uri(), true);
    server80.send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server80.client().stop();             // Stop is needed because we sent no content length
  });
  server80.begin(); //mac dinh port 80  
  /*=================================================================
     Check Activated
    =================================================================*/
  FileConfig.ConfigActivated(Df_ConfigActiveRead);  
}

void loop_main(void){
  /*=================================================================
    Web Socket Loop
    =================================================================*/
  webSocket.loop();
  /* Timelive Ws toàn bộ các Page */
  WsTimeLive();
  /*=================================================================
    Web Server
    =================================================================*/
  server.handleClient();
  server80.handleClient();
  /*=================================================================
    Blink Led
    =================================================================*/
  LED_STT.Blink();
  BUZZ_STT.Blink();
  /*=================================================================
    Manager Wifi
    =================================================================*/
  WifiConfig.WifiConnectStatusProc();
  /*=================================================================
    Button Factory
  =================================================================*/
  TaskFactoryProc();
  /*=================================================================
    Timeout Reset Esp
    =================================================================*/
  if (ESPRebootTo.ToEExpired())
  {
    COM_DEBUG("Esp Rebooting...");
    delay(100);
    ESP.restart();
  }  
}

void setup()
{     
  Init_Setup();
}

void loop()
{  
  loop_main();
}
