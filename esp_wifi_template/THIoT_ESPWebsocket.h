#ifndef	__ESP_WEBSOCKET_H
#define __ESP_WEBSOCKET_H

#include <Arduino.h>
#include <functional>
#include <ESPAsyncWebServer.h>
#include "THIoT_PFTicker.h"

#define NUM_WS_CONNECTION_MAX       3
#define WS_INTERVAL_TIMEOUT_NUM     10 /* S */
#if (DEFAULT_MAX_WS_CLIENTS < NUM_WS_CONNECTION_MAX)
#error NUM_WS_CONNECTION_MAX value wrong
#endif

typedef enum {
    ASYNC_WS_DISCONNECT,
    ASYNC_WS_CONNECTED
} ws_connection_status_t;

typedef struct {
    uint32_t timestamp;
    uint8_t ws_num;
    uint8_t status;
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
    static dataSocketHandler _dataHandler;
    static WebsocketCallbacks* _pCallbacks;
    static AsyncWebSocket _asyncWs;
    static AsyncEventSource _asyncEvents;
    static ws_connection_info_t _ws_connection[NUM_WS_CONNECTION_MAX];
    static void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
    static void disconnect(uint8_t ws_index);
    static uint8_t connectionAvailable(void);
    static void connectionEstablish(uint8_t ws_num);
    static void connectionRemove(uint8_t ws_num);
    static uint8_t connectionHasTimeLiveMax(void);
    static void intervalCleanUpClients(void);
    AsyncWebSocketMessageBuffer *rootBuffer;

public:
    ESPWebsocket();
    ~ESPWebsocket();
    void begin();
    void end();
    void onDataHandler(dataSocketHandler handler);
    void sendTxt(uint8_t ws_index, char *payload);
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

    static uint8_t connectedNumber(void); // ws_connection_connected
    static uint8_t clientIdIsConnected(uint8_t index); // ws_index_is_connected
    static uint8_t clientIdGet(uint8_t index); // ws_index_num_get
};

extern ESPWebsocket wsHandler;

#endif // __ESP_WEBSOCKET_H
