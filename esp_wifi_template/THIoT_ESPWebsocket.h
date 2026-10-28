#ifndef	__ESP_WEBSOCKET_H
#define __ESP_WEBSOCKET_H

#include <Arduino.h>
#include <functional>
#include <ESPAsyncWebServer.h>
#include "THIoT_PFTicker.h"

#define NUM_WS_CONNECTION_MAX       2U
/* Ev num should higher than ws num at lead 1 unit */
#define NUM_EVT_CONNECTION_MAX      (NUM_WS_CONNECTION_MAX + 1) /* DEFAULT_MAX_WS_CLIENTS(8) is maximum */

#if (DEFAULT_MAX_WS_CLIENTS < NUM_WS_CONNECTION_MAX)
#error NUM_WS_CONNECTION_MAX value wrong
#endif

#define WS_CLEAN_UP_INTERVAL        5U /* second */

typedef enum {
    ASYNC_WS_DISCONNECT,
    ASYNC_WS_CONNECTED
} ws_connection_status_t;

typedef enum : uint8_t {
  P_NULL  = 0,          /*!< No Page*/
  P_HOME  = 1,          /*!< Page home.htm display*/
  P_INDEX = 2,          /*!< Page home.htm Fn*/
  P_CARD_READER = 3,    /*!< Page card_reader.htm Fn*/
  P_WS_CONSOLE = 4      /*!< Page ws_console.htm Fn*/
} ws_page_connect_t;

typedef struct {
    uint32_t timestamp;
    uint8_t ws_num;
    uint8_t status;
    uint8_t page;
    uint8_t pump;
    uint8_t home_seen;  /* 1 once this socket watched P_HOME (home.htm tab open) */
} ws_connection_info_t;

/**
 * Params 1: client
 * Params 2: data received
*/
typedef std::function<void(AsyncWebSocketClient*, char*)> dataSocketHandler;
typedef std::function<void(uint32_t)> wsNewEventHandle;

class WebsocketCallbacks {
public:
	virtual ~WebsocketCallbacks();
    /**
    * Handler called after once received data.
    */
    virtual void onDataReceived(AsyncWebSocketClient* client, char* data);
};

class ESPWebsocket
{
private:
    static wsNewEventHandle _wsNewEventCallback;
    static ticker_function_handle_t _ws_ticker;
    static ticker_t _evConnectionInhibit;
    static ticker_t _wsConnectionInhibit;
    static ticker_t _evCloseAll;
    static ticker_t _wsCloseAll;
    static dataSocketHandler _dataHandler;
    static WebsocketCallbacks* _pCallbacks;
    static AsyncWebSocket _asyncWs;
    static AsyncEventSource _asyncEvents;
    static int _eventNum;
    static ws_connection_info_t _ws_connection[NUM_WS_CONNECTION_MAX];
    static void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
    static void disconnect(uint8_t ws_index);
    static uint8_t connectionAvailable();
    static void connectionEstablish(uint8_t ws_num);
    static void connectionRemove(uint8_t ws_num);
    static void connectionClean();
    static uint8_t connectionHasTimeLiveMax();
    static void intervalCleanUpClients();

public:
    ESPWebsocket();
    ~ESPWebsocket();
    void begin();
    void end();
    void onDataHandler(dataSocketHandler handler);
    void sendTxt(uint8_t ws_index, char *payload);
    void sendTxt(uint8_t ws_index, AsyncWebSocketMessageBuffer *payload);
    void sendBroadcastTxt(char *payload);
    void sendBroadcastTxt(AsyncWebSocketMessageBuffer *payload);
    void setHandleCallbacks(WebsocketCallbacks* pCallbacks);
    void eventsSend(const char *message, const char *event)
    {
        _asyncEvents.send(message, event);
    }
    void OnNewEventConnection(wsNewEventHandle cb)
    {
        _wsNewEventCallback = cb;
    }
    AsyncWebSocket& asyncWs() { return _asyncWs; }
    AsyncEventSource& asyncEvents() { return _asyncEvents; }

    static void disconnectOldestOnce();
    static uint8_t connectedNumber(); // ws_connection_connected
    static uint16_t pageIndexConnected(ws_page_connect_t Page); // ws_page_index_connected
    static bool homePageActive(void); // ws_home_page_active (current P_HOME or visited)
    static void pageTypeUpdate(uint8_t ws_num, ws_page_connect_t Page); // ws_page_update
    static bool pageTypeSend(AsyncWebSocketMessageBuffer *payload, char *Data, ws_page_connect_t Page, size_t length = 0); // ws_page_send
#if (0)
    static bool pageTypeSend(AsyncWebSocketSharedBuffer payload, char *Data, ws_page_connect_t Page); // ws_page_send
#endif
    static void pumpIdUpdate(uint8_t ws_num, uint8_t Pump); // ws_pump_update
    static uint8_t clientIdIsConnected(uint8_t index); // ws_index_is_connected
    static uint8_t pageTypeGet(uint8_t Page); // ws_index_page_get
    static bool isPageType(uint8_t ws_num, ws_page_connect_t Page);
    static int8_t socketIndex(uint8_t ws_num);
    static uint8_t pageTypeSocket(uint8_t ws_num);
    static uint8_t clientIdGet(uint8_t index); // ws_index_num_get
    static uint8_t pumpIdGet(uint8_t index); // ws_index_pump_get
};

extern ESPWebsocket wsHandler;

#endif // __ESP_WEBSOCKET_H
