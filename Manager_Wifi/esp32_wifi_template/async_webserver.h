#ifndef	_ASYNC_WEB_SERVER_H
#define _ASYNC_WEB_SERVER_H

#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

char* http_username;
char* http_password;

void web_server_setup(void);
void web_server_init(void);
void web_server_end(void);

#endif
