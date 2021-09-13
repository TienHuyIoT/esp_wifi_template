#ifndef	_ASYNC_WEB_SERVER_H
#define _ASYNC_WEB_SERVER_H

/*
Edit 
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

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "app_config.h"
#include "fs_editor.h"
#include "fs_handle.h"

typedef enum {
  HTTP_AUTH_LV0 = 0,
  HTTP_AUTH_LV1,
  HTTP_AUTH_LV2,
  HTTP_AUTH_FAIL
} http_auth_level_t;

class async_webserver
{
private:
  static AsyncWebServer* server;
  static AsyncWebServer* server80;
  static FSEditor* spiffs_editor;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  static FSEditor* sd_editor;
  static uint32_t sdFsPersent;
#endif
  static String http_username;
  static String http_password;
  static String http_username1;
  static String http_password1;
  static int update_cmd;
  static uint32_t spiffsPercent;
  static uint32_t updatePercent;

  static uint8_t authentication_level(AsyncWebServerRequest *request);
  static void update_printProgress(size_t prg, size_t sz);
  static void spiffs_printProgress(size_t prg, size_t sz);
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  static void sdfs_printProgress(size_t prg, size_t sz);
#endif
  static void fs_editor_status(AsyncWebServerRequest *request);

public:
  async_webserver(/* args */);
  ~async_webserver();

  void begin();
  void loop();
  void end() {
    delete server;
    delete server80;
    delete spiffs_editor;
  }
};

#endif
