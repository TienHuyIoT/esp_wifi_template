#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPWebsocket.h"
#include "THIoT_APPConfig.h"

#define WS_DBG_PORT SERIAL_PORT
#define WS_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[WS]", __VA_ARGS__)
#define WS_FUNCTION_TAG_CONSOLE(...) SERIAL_FUNCTION_TAG_LOGI("[WS]", __VA_ARGS__)

const char* wsUrl = "/ws";
const char* eventUrl = "/events";

static WebsocketCallbacks defaultCallbacks; //null-object-pattern

ESPWebsocket::ESPWebsocket()
{
    memset(_ws_connection, 0, sizeof(_ws_connection));
}

ESPWebsocket::~ESPWebsocket()
{
    this->end();
}

wsNewEventHandle ESPWebsocket::_wsNewEventCallback = nullptr;
ticker_function_handle_t ESPWebsocket::_ws_ticker = {0};
AsyncWebSocket ESPWebsocket::_asyncWs = AsyncWebSocket(wsUrl);
AsyncEventSource ESPWebsocket::_asyncEvents = AsyncEventSource(eventUrl);
WebsocketCallbacks* ESPWebsocket::_pCallbacks = nullptr;
dataSocketHandler ESPWebsocket::_dataHandler = nullptr;
ws_connection_info_t ESPWebsocket::_ws_connection[NUM_WS_CONNECTION_MAX] = {0};

void ESPWebsocket::setHandleCallbacks(WebsocketCallbacks* pCallbacks)
{
    if (pCallbacks != nullptr)
    {
        _pCallbacks = pCallbacks;
    }
    else
    {
        _pCallbacks = &defaultCallbacks;
    }
}

void ESPWebsocket::end() {}

void ESPWebsocket::begin()
{
    _asyncWs.onEvent(onWsEvent);
    _asyncEvents.onConnect([](AsyncEventSourceClient *client){
        if(client->lastId()) {
            WS_FUNCTION_TAG_CONSOLE("Client reconnected! Last message ID that it got is: %u", client->lastId());
        }
        //send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("hello!", NULL, millis(), 1000);

        if (_wsNewEventCallback)
        {
            WS_FUNCTION_TAG_CONSOLE("New Event establish");
            _wsNewEventCallback(client->lastId());
        }
    });

    ticker_attach(&_ws_ticker, WS_INTERVAL_TIMEOUT_NUM, [](void *arg){
        ESPWebsocket *handler = (ESPWebsocket*)arg;
        handler->intervalCleanUpClients();
    }, this);
}

/* send message to client */
void ESPWebsocket::sendTxt(uint8_t ws_index, char *payload)
{
    _asyncWs.text(ws_index, payload);
}

/* send data to all connected clients */
void ESPWebsocket::sendBroadcastTxt(char *payload)
{
    _asyncWs.textAll(payload);
}

void ESPWebsocket::sendBroadcastTxt(AsyncWebSocketMessageBuffer *payload)
{
    _asyncWs.textAll(payload);
}

void ESPWebsocket::intervalCleanUpClients(void)
{
    uint8_t ws_cnt;

    ws_cnt = connectedNumber();
    if (ws_cnt)
    {
#if (0) // for debug
        WS_FUNCTION_TAG_CONSOLE("connectedNumber: %u", ws_cnt);
        WS_FUNCTION_TAG_CONSOLE("Heap: %u", ESP.getFreeHeap());
#endif
    }

    _asyncWs.cleanupClients();
}

/* Init and register callback receive message */
void ESPWebsocket::onDataHandler(dataSocketHandler handler)
{
    _dataHandler = handler;
}

void ESPWebsocket::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        IPAddress ip = client->remoteIP();
        uint16_t port = client->remotePort();
        WS_FUNCTION_TAG_CONSOLE("[%u] Connected from %d.%d.%d.%d port: %u url: %s\n", client->id(), ip[0], ip[1], ip[2], ip[3], port, server->url());
        /* NUM_WS_CONNECTION_MAX < DEFAULT_MAX_WS_CLIENTS
         * Because at least have a socket connection free
         */
        if (0 == connectionAvailable())
        {
            /* Disconnect socket has timelive max */
            disconnect(connectionHasTimeLiveMax());
        }

        /* new connection establish */
        connectionEstablish(client->id());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        WS_FUNCTION_TAG_CONSOLE("_asyncWs[%s][%u] disconnect\n", server->url(), client->id());
        connectionRemove(client->id());
    }
    else if (type == WS_EVT_ERROR)
    {
        WS_FUNCTION_TAG_CONSOLE("_asyncWs[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
    }
    else if (type == WS_EVT_PONG)
    {
        WS_FUNCTION_TAG_CONSOLE("_asyncWs[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        String msg = "";
        if (info->final && info->index == 0 && info->len == len)
        {
            //the whole message is in a single frame and we got all of it's data
            WS_FUNCTION_TAG_CONSOLE("_asyncWs[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

            if (info->opcode == WS_TEXT)
            {
                for (size_t i = 0; i < info->len; i++)
                {
                    msg += (char)data[i];
                }
                WS_FUNCTION_TAG_CONSOLE("%s\n", msg.c_str());
                /* Call callback */
                if (_dataHandler)
                {
                    _dataHandler(client, (char *)msg.c_str());
                }

                _pCallbacks->onDataReceived(client, (char *)msg.c_str());
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
            WS_FUNCTION_TAG_CONSOLE("%s\n", msg.c_str());
        }
        else
        {
            //message is comprised of multiple frames or the frame is split into multiple packets
            if (info->index == 0)
            {
                if (info->num == 0)
                    WS_FUNCTION_TAG_CONSOLE("_asyncWs[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
                WS_FUNCTION_TAG_CONSOLE("_asyncWs[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
            }

            WS_FUNCTION_TAG_CONSOLE("_asyncWs[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

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
            WS_FUNCTION_TAG_CONSOLE("%s\n", msg.c_str());

            if ((info->index + len) == info->len)
            {
                WS_FUNCTION_TAG_CONSOLE("_asyncWs[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
                if (info->final)
                {
                    WS_FUNCTION_TAG_CONSOLE("_asyncWs[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
                    if (info->message_opcode == WS_TEXT)
                    {
                        /* Call callback */
                        if (_dataHandler)
                        {
                            _dataHandler(client, (char *)msg.c_str());
                        }

                        _pCallbacks->onDataReceived(client, (char *)msg.c_str());
                    }
                }
            }
        }
    }
}

/* Disconnect */
void ESPWebsocket::disconnect(uint8_t ws_index)
{
    _asyncWs.close(_ws_connection[ws_index].ws_num);
    _ws_connection[ws_index].status = ASYNC_WS_DISCONNECT;
    _ws_connection[ws_index].ws_num = NUM_WS_CONNECTION_MAX;
    _ws_connection[ws_index].timestamp = millis();
    WS_FUNCTION_TAG_CONSOLE("Disconnect ws_num %u at %u", _ws_connection[ws_index].ws_num, ws_index);
}

/* Add connection */
void ESPWebsocket::connectionEstablish(uint8_t ws_num)
{
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (ASYNC_WS_DISCONNECT == _ws_connection[i].status)
        {
            WS_FUNCTION_TAG_CONSOLE("Establish ws_num %u at %u", ws_num, i);
            _ws_connection[i].status = ASYNC_WS_CONNECTED;
            _ws_connection[i].ws_num = ws_num;
            _ws_connection[i].timestamp = millis();
            break;
        }
    }
}

/* Remove connection */
void ESPWebsocket::connectionRemove(uint8_t ws_num)
{
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (ws_num == _ws_connection[i].ws_num)
        {
            WS_FUNCTION_TAG_CONSOLE("Remove ws_num %u at %u", ws_num, i);
            _ws_connection[i].status = ASYNC_WS_DISCONNECT;
            _ws_connection[i].ws_num = NUM_WS_CONNECTION_MAX;
            _ws_connection[i].timestamp = millis();
            break;
        }
    }
}

/* Return the number socket free
 * 0: There are not socket index to establish
 */
uint8_t ESPWebsocket::connectionAvailable(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (ASYNC_WS_DISCONNECT == _ws_connection[i].status)
        {
            ++count;
        }
    }

    WS_FUNCTION_TAG_CONSOLE("Available %u", count);
    return count;
}

/* Return the number socket is connected
 * 0: There are not socket index connected
 */
uint8_t ESPWebsocket::connectedNumber(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (ASYNC_WS_CONNECTED == _ws_connection[i].status)
        {
            ++count;
        }
    }

    // WS_FUNCTION_TAG_CONSOLE("Connected %u", count);
    return count;
}

/* Brief: return websocket index in array_list has timelive max */
uint8_t ESPWebsocket::connectionHasTimeLiveMax(void)
{
    uint32_t tl_sub;
    uint32_t tl_max = 0;
    uint32_t now = millis();
    uint8_t ws_index = 0;
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        tl_sub = now - _ws_connection[i].timestamp;
        if (tl_sub >= tl_max)
        {
            tl_max = tl_sub;
            ws_index = i;
        }
    }

    WS_FUNCTION_TAG_CONSOLE("TimeLive max is ws_num %u at %u(%us)",
                 _ws_connection[ws_index].ws_num, ws_index, tl_max / 1000);

    return ws_index;
}

uint8_t ESPWebsocket::clientIdIsConnected(uint8_t index)
{
    return (ASYNC_WS_CONNECTED == _ws_connection[index].status);
}

uint8_t ESPWebsocket::clientIdGet(uint8_t index)
{
    return _ws_connection[index].ws_num;
}

WebsocketCallbacks::~WebsocketCallbacks() {}
/**
* Handler called after once received data.
*/
void WebsocketCallbacks::onDataReceived(AsyncWebSocketClient* client, char* data)
{
    WS_FUNCTION_TAG_CONSOLE("[WebsocketCallbacks] >> onDataReceived: default <<");
}

ESPWebsocket wsHandler;