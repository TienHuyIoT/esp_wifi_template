#ifndef	__ESP_WS_DATA_HANDLER_H
#define __ESP_WS_DATA_HANDLER_H

#include <Arduino.h>
#include "THIoT_ESPWebsocket.h"

typedef enum {
    WS_PAGE_CARD_HISTORY = 0,
    WS_PAGE_MANAGE,
    WS_PAGE_CARD_INFO
} ws_page_num_t;

typedef enum {
    PAGE_CMD_CONNECT = 0,
    PAGE_CMD_RECEIVE_DATA,
    PAGE_CMD_REPLY_DATA,
    PAGE_CMD_ASYCN_DATA
} page_card_user_cmd_t;

typedef enum {
    WS_STATUS_ERROR = 0,
    WS_STATUS_OK
} ws_status_t;

const char *const ws_page_list[] PROGMEM = {
	"WS_PAGE_CARD_HISTORY",	
	"WS_PAGE_MANAGE",
	"WS_PAGE_CARD_INFO"
};

const char *const page_card_user_list[] PROGMEM = {
	"PAGE_CMD_CONNECT",	
	"PAGE_CMD_RECEIVE_DATA",
	"PAGE_CMD_REPLY_DATA",
    "PAGE_CMD_ASYCN_DATA"
};

class ESPWsDataHandle : public WebsocketCallbacks
{
private:
    void onDataReceived(AsyncWebSocketClient* client, char* data);
public:
    ESPWsDataHandle(/* args */);
    ~ESPWsDataHandle();
};

#endif // __ESP_WS_DATA_HANDLER_H
