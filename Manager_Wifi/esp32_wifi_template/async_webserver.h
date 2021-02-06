#ifndef	_ASYNC_WEB_SERVER_H
#define _ASYNC_WEB_SERVER_H

#include <ESPAsyncWebServer.h>

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

typedef enum {
  HTTP_AUTH_LV0 = 0,
  HTTP_AUTH_LV1,
  HTTP_AUTH_LV2,
  HTTP_AUTH_FAIL
} http_auth_level_t;

AsyncWebServer server(25123);
AsyncWebServer server80(80);

FSEditor spiffs_editor(NAND_FS_SYSTEM, "/edit");
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
FSEditor sd_editor(SD_FS_SYSTEM, "/edit_sdfs");
#endif
size_t update_content_len;

char* http_username;
char* http_password;
char* http_username1;
char* http_password1;

void web_server_setup(void);
void web_server_init(void);
void web_server_end(void);

#endif
