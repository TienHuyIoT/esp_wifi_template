#include "WebSocket.h"
#include "wifi_data_file.h"

#define WS_DBG_PORT Serial
#define WS_DBG_PRINT(fmt, ...) WS_DBG_PORT.printf_P(PSTR("\r\n<WS> " fmt), ##__VA_ARGS__)

/* Callback receive message from client */
void (*ws_txt_callback)(uint8_t ws_index, uint8_t *payload);

/* send message to client */
void ws_send_txt(uint8_t ws_index, uint8_t *payload)
{
    webSocket.sendTXT(ws_index, payload);
}

/* send data to all connected clients */
void ws_send_broadcast_txt(uint8_t *payload)
{
    webSocket.broadcastTXT(payload);
}

void ws_interval_sync(void)
{
    uint8_t ws_cnt;
    /* Waiting timeout */
    if (!ws_interval_broadcast.ToEExpired())
    {
        return;
    }
    /* Update Timout 10s */
    ws_interval_broadcast.ToEUpdate(WS_INTERVAL_TIMEOUT_NUM);

    ws_cnt = ws_connection_connected();
    if (ws_cnt)
    {
        webSocket.broadcastTXT("{\"page\":100,\"socket_num\":" + String(ws_cnt) + "}");
    }
}

/* Init and register callback receive message */
void web_socket_init(void (*cb)(uint8_t, uint8_t *))
{
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();
    ws_txt_callback = cb;
    /*
    Load New Port. must be call newport after begin()
    Add in WebSocketsServer.cpp
    void WebSocketsServer::NewPort(int port) {
        _server->begin(port);
    }
    Add in WebSocketsServer.h
    void NewPort(int port = 80);
    */
    webSocket.begin();

    if (g_wifi_cfg->WSPort <= 80)
    {
        g_wifi_cfg->WSPort = 25124;
    }

    if (g_wifi_cfg->WSPort == g_wifi_cfg->TCPPort)
    {
        g_wifi_cfg->WSPort++;
    }

    if (g_wifi_cfg->WSPort != 25124)
    {
        webSocket.NewPort(g_wifi_cfg->WSPort);
    }

    WS_DBG_PRINT("\r\nInit Web Socket Port: %u\r\n", g_wifi_cfg->WSPort);
    /*  */
    webSocket.onEvent(webSocketEvent);
}

/* ws event callback */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{

    switch (type)
    {
    case WStype_DISCONNECTED:
        WS_DBG_PRINT("[%u] Disconnected!\n", num);
        ws_connection_remove(num);
        break;

    case WStype_CONNECTED:
    {
        IPAddress ip = webSocket.remoteIP(num);
        WS_DBG_PRINT("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        /* NUM_WS_CONNECTION_MAX < WEBSOCKETS_SERVER_CLIENT_MAX 
         * Because at least have a socket connection free
         */
        if (0 == ws_connection_available())
        {
            /* Disconnect socket has timelive max */
            ws_disconnect(ws_connection_index_has_tl_max());
        }

        /* new connection establish */
        ws_connection_establish(num);

        webSocket.broadcastTXT("{\"page\":100,\"socket_num\":" + String(ws_connection_connected()) + "}");
    }
    break;

    case WStype_TEXT:
        WS_DBG_PRINT("[%u] get Text(%u): %s\n", num, length, payload);

        /* Call callback */
        if (ws_txt_callback)
        {
            ws_txt_callback(num, payload);
        }
        break;

    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
        break;
    }
}

/* Disconnect */
void ws_disconnect(uint8_t ws_index)
{
    webSocket.disconnect(ws_connection[ws_index].ws_num);
    ws_connection[ws_index].status = WS_DISCONNECT;
    ws_connection[ws_index].ws_num = NUM_WS_CONNECTION_MAX;
    ws_connection[ws_index].timestamp = millis();
    WS_DBG_PRINT("Disconnect ws_num %u at %u", ws_connection[ws_index].ws_num, ws_index);
}

/* Add connection */
void ws_connection_establish(uint8_t ws_num)
{
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (WS_DISCONNECT == ws_connection[i].status)
        {
            WS_DBG_PRINT("Establish index %u at %u", ws_num, i);
            ws_connection[i].status = WS_CONNECTED;
            ws_connection[i].ws_num = ws_num;
            ws_connection[i].timestamp = millis();
            break;
        }
    }
}

/* Remove connection */
void ws_connection_remove(uint8_t ws_num)
{
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (ws_num == ws_connection[i].ws_num)
        {
            WS_DBG_PRINT("Remove index %u at %u", ws_num, i);
            ws_connection[i].status = WS_DISCONNECT;
            ws_connection[i].ws_num = NUM_WS_CONNECTION_MAX;
            ws_connection[i].timestamp = millis();
            break;
        }
    }
}

/* Return the number socket free 
 * 0: There are not socket index to establish
 */
uint8_t ws_connection_available(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (WS_DISCONNECT == ws_connection[i].status)
        {
            ++count;
        }
    }

    WS_DBG_PRINT("Available %u", count);
    return count;
}

/* Return the number socket is connected
 * 0: There are not socket index connected
 */
uint8_t ws_connection_connected(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (WS_CONNECTED == ws_connection[i].status)
        {
            ++count;
        }
    }

    WS_DBG_PRINT("Connected %u", count);
    return count;
}

/* Brief: return websocket index in array_list has timelive max */
uint8_t ws_connection_index_has_tl_max(void)
{
    uint32_t tl_sub;
    uint32_t tl_max = 0;
    uint32_t now = millis();
    uint8_t ws_index = 0;
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        tl_sub = now - ws_connection[i].timestamp;
        if (tl_sub >= tl_max)
        {
            tl_max = tl_sub;
            ws_index = i;
        }
    }

    WS_DBG_PRINT("Timelive max is ws_num %u at %u(%us)",
                 ws_connection[ws_index].ws_num, ws_index, tl_max / 1000);

    return ws_index;
}