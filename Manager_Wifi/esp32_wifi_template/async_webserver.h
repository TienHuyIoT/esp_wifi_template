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
AsyncWebServer server(25123);
AsyncWebServer server80(80);

char* http_username;
char* http_password;

void web_server_setup(void);
void web_server_init(void);
void web_server_end(void);

#endif
