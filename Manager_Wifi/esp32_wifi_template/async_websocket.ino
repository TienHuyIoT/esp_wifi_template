#include "async_websocket.h"
#include "wifi_data_file.h"

#define WS_DBG_PORT Serial
#define WS_DBG_PRINT(fmt, ...) WS_DBG_PORT.printf_P(PSTR("\r\n[WS] " fmt), ##__VA_ARGS__)

/* Callback receive message from client */
void (*ws_txt_callback)(uint8_t ws_index, char *payload);

/* send message to client */
void ws_send_txt(uint8_t ws_index, char *payload)
{
    ws.text(ws_index, payload);
}

/* send data to all connected clients */
void ws_send_broadcast_txt(char *payload)
{
    ws.textAll(payload);
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
        ws.textAll("{\"page\":100,\"socket_num\":" + String(ws_cnt) + "}");
    }
}

/* Init and register callback receive message */
void web_socket_init(void (*cb)(uint8_t, char *))
{
    ws_txt_callback = cb;
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        IPAddress ip = client->remoteIP();
        uint16_t port = client->remotePort();
        WS_DBG_PRINT("[%u] Connected from %d.%d.%d.%d port: %u url: %s\n", client->id(), ip[0], ip[1], ip[2], ip[3], port, server->url());
        /* NUM_WS_CONNECTION_MAX < DEFAULT_MAX_WS_CLIENTS 
         * Because at least have a socket connection free
         */
        if (0 == ws_connection_available())
        {
            /* Disconnect socket has timelive max */
            ws_disconnect(ws_connection_index_has_tl_max());
        }

        /* new connection establish */
        ws_connection_establish(client->id());
        client->ping();

        server->textAll("{\"page\":100,\"socket_num\":" + String(ws_connection_connected()) + "}");
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        WS_DBG_PRINT("ws[%s][%u] disconnect\n", server->url(), client->id());
        ws_connection_remove(client->id());
    }
    else if (type == WS_EVT_ERROR)
    {
        WS_DBG_PRINT("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
    }
    else if (type == WS_EVT_PONG)
    {
        WS_DBG_PRINT("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        String msg = "";
        if (info->final && info->index == 0 && info->len == len)
        {
            //the whole message is in a single frame and we got all of it's data
            WS_DBG_PRINT("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

            if (info->opcode == WS_TEXT)
            {
                for (size_t i = 0; i < info->len; i++)
                {
                    msg += (char)data[i];
                }
                /* Call callback */
                if (ws_txt_callback)
                {
                    ws_txt_callback(client->id(), (char*)msg.c_str());
                }
            }
            else
            {
                char buff[3];
                for (size_t i = 0; i < info->len; i++)
                {
                    sprintf(buff, "%02x ", (uint8_t)data[i]);
                    msg += buff;
                }
            }
            WS_DBG_PRINT("%s\n", msg.c_str());            
        }
        else
        {
            //message is comprised of multiple frames or the frame is split into multiple packets
            if (info->index == 0)
            {
                if (info->num == 0)
                    WS_DBG_PRINT("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
                WS_DBG_PRINT("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
            }

            WS_DBG_PRINT("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

            if (info->opcode == WS_TEXT)
            {
                for (size_t i = 0; i < len; i++)
                {
                    msg += (char)data[i];
                }
            }
            else
            {
                char buff[3];
                for (size_t i = 0; i < len; i++)
                {
                    sprintf(buff, "%02x ", (uint8_t)data[i]);
                    msg += buff;
                }
            }
            WS_DBG_PRINT("%s\n", msg.c_str());

            if ((info->index + len) == info->len)
            {
                WS_DBG_PRINT("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
                if (info->final)
                {
                    WS_DBG_PRINT("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
                    if (info->message_opcode == WS_TEXT)
                    {
                        /* Call callback */
                        if (ws_txt_callback)
                        {
                            ws_txt_callback(client->id(), (char*)msg.c_str());
                        }
                    }
                }
            }
        }
    }
}

/* Disconnect */
void ws_disconnect(uint8_t ws_index)
{
    ws.close(ws_connection[ws_index].ws_num);
    ws_connection[ws_index].status = ASYNC_WS_DISCONNECT;
    ws_connection[ws_index].ws_num = NUM_WS_CONNECTION_MAX;
    ws_connection[ws_index].timestamp = millis();
    WS_DBG_PRINT("Disconnect ws_num %u at %u", ws_connection[ws_index].ws_num, ws_index);
}

/* Add connection */
void ws_connection_establish(uint8_t ws_num)
{
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (ASYNC_WS_DISCONNECT == ws_connection[i].status)
        {
            WS_DBG_PRINT("Establish index %u at %u", ws_num, i);
            ws_connection[i].status = ASYNC_WS_CONNECTED;
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
            ws_connection[i].status = ASYNC_WS_DISCONNECT;
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
        if (ASYNC_WS_DISCONNECT == ws_connection[i].status)
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
        if (ASYNC_WS_CONNECTED == ws_connection[i].status)
        {
            ++count;
        }
    }

    // WS_DBG_PRINT("Connected %u", count);
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