#ifndef	_ASYNC_WEB_SOCKET_H
#define _ASYNC_WEB_SOCKET_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>

#define NUM_WS_CONNECTION_MAX   (DEFAULT_MAX_WS_CLIENTS - 3)
#define WS_INTERVAL_TIMEOUT_NUM  60 /* S */

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

class async_websocket
{
private:
    static Ticker* _ws_ticker;
    static AsyncWebSocket* _ws;
    static AsyncEventSource* _events;
    static dataSocketHandler _dataHandler;
    static ws_connection_info_t _ws_connection[NUM_WS_CONNECTION_MAX];
    static void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
    static void ws_disconnect(uint8_t ws_index);
    static void ws_connection_establish(uint8_t ws_num);
    static void ws_connection_remove(uint8_t ws_num);
    static uint8_t ws_connection_available(void);
    static uint8_t ws_connection_connected(void);
    static uint8_t ws_connection_index_has_tl_max(void);
    static void ws_interval_sync(void);

    String _wsUrl;
    String _eventUrl;
public:
    async_websocket(const String& ws, const String& event);
    ~async_websocket();
    
    void begin();
    void end();
    void onDataHandler(dataSocketHandler handler);
    void ws_send_txt(uint8_t ws_index, char *payload);
    void ws_send_broadcast_txt(char *payload);
};

#endif
