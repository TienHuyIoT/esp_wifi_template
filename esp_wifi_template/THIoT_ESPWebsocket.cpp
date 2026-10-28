#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_ESPWebsocket.h"
#include "THIoT_APPConfig.h"
#include "THIoT_ESPWsDataHandler.h"

#define WS_DBG_PORT SERIAL_PORT
#define EV_TAG_SERIAL(...)                  //SERIAL_TAG_LOGI("[events/]", __VA_ARGS__)
#define WS_TAG_CONSOLE(...)                 //CONSOLE_TAG_LOGI("[/ws]", __VA_ARGS__)
#define WS_TAG_SERIAL(...)                  //SERIAL_TAG_LOGI("[/ws]", __VA_ARGS__)
#define WS_SEND_TAG_SERIAL(...)             //SERIAL_TAG_LOGI("[/ws]", __VA_ARGS__)

#define WS_TRACE_TAG_LOG(...)               FS_TAG_LOGI("[/ws]", __VA_ARGS__)
#define EV_TRACE_TAG_LOG(...)               //FS_TAG_LOGI("[/events]", __VA_ARGS__)

/* Only for event socket */
#define ASYNC_EV_RECONNECT_DELAY            1000U         /* Unit: ms */
/* For event socket */
#define ASYNC_EV_DISCONNECT_DELAY           10000U        /* Unit: ms */
#define ASYNC_EV_CLOSE_ALL_TIMEOUT          (uint32_t)(15 * 60000) /* Unit: ms */

/* For ws socket */
#define ASYNC_WS_DISCONNECT_DELAY           10000U        /* Unit: ms */
#define ASYNC_WS_CLOSE_ALL_TIMEOUT          (uint32_t)(4 * 60 * 60000) /* Unit: ms */

/* For event/ws socket cleanup */
#define WS_EV_CONNECTED_CLEAN_UP_TIMEOUT    1000U        /* Unit: ms */

const char *const page_type_str[] PROGMEM{
    "P_NULL",
    "P_HOME",
    "P_INDEX",
    "P_CARD_READER",
    "P_WS_CONSOLE"};

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
ticker_t ESPWebsocket::_evConnectionInhibit = TICKER_STOP_INIT;
ticker_t ESPWebsocket::_wsConnectionInhibit = TICKER_STOP_INIT;
ticker_t ESPWebsocket::_evCloseAll = TICKER_STOP_INIT;
ticker_t ESPWebsocket::_wsCloseAll = TICKER_STOP_INIT;
AsyncWebSocket ESPWebsocket::_asyncWs(wsUrl);
AsyncEventSource ESPWebsocket::_asyncEvents(eventUrl);
WebsocketCallbacks* ESPWebsocket::_pCallbacks = nullptr;
dataSocketHandler ESPWebsocket::_dataHandler = nullptr;
ws_connection_info_t ESPWebsocket::_ws_connection[NUM_WS_CONNECTION_MAX] = {0};
int ESPWebsocket::_eventNum = 0;

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
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
    _asyncWs.addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
        size_t num = _asyncWs.count();
        WS_TRACE_TAG_LOG("onMiddle count(): %u", num + 1);

        if (ticker_status(&_wsConnectionInhibit) == TICKER_RUNNING) {
            // too many clients - answer back immediately and stop processing next middlewares and handler
            WS_TRACE_TAG_LOG("onMiddle connection inhibited. Server is busy");
            request->send(503, "text/plain", "Server is busy");
            return;
        }

        if ((num + 1) > NUM_WS_CONNECTION_MAX) {
            WS_TRACE_TAG_LOG("onMiddle too many clients %u/%u, --> close connection of the oldest client", num + 1, NUM_WS_CONNECTION_MAX);
            if (0 == connectionAvailable()) {
                disconnectOldestOnce();
            }
        } 
        // process next middleware and at the end the handler
        next();
    });
#endif

    _asyncEvents.onConnect([](AsyncEventSourceClient *client) {
        ++_eventNum;
        IPAddress ip = client->client()->remoteIP();
        uint16_t port = client->client()->remotePort();
        if(client->lastId()) {
            EV_TAG_SERIAL("onConnect reconnected! Last message ID that it got is: %u", client->lastId());
        }
        //send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("hello!", NULL, millis(), ASYNC_EV_RECONNECT_DELAY);

        if (_wsNewEventCallback) {
            _wsNewEventCallback(client->lastId());
        }

        /* Count the number connection of clients with SSE_CLIENT_QUEUE_UNLOCK
            Due to the calling is performed on a callback function connected.
        */
        uint8_t evt_cnt = _asyncEvents.count(SSE_CLIENT_QUEUE_UNLOCK);
        EV_TRACE_TAG_LOG("onConnect count(): %u", evt_cnt);
        if (evt_cnt > NUM_EVT_CONNECTION_MAX) {
            EV_TRACE_TAG_LOG("onConnect close 1/%u/%u connection of the oldest client", evt_cnt, NUM_EVT_CONNECTION_MAX);
            /* Close 1 connection of the oldest client with SSE_CLIENT_QUEUE_UNLOCK
                Due to the calling is performed on a callback function connected.
            */
            _asyncEvents.close(1, SSE_CLIENT_QUEUE_UNLOCK);
            ticker_begin(&_evConnectionInhibit, ASYNC_EV_DISCONNECT_DELAY);
        }
        EV_TRACE_TAG_LOG("onConnect from %d.%d.%d.%d port: %u, client_ID = %u", ip[0], ip[1], ip[2], ip[3], port, client->id());
        ticker_restart(&_evCloseAll);
        ticker_attach_ms_trigger(&_ws_ticker, WS_EV_CONNECTED_CLEAN_UP_TIMEOUT);
    });

    /* The client pointer sent to the callback is only for reference purposes. DO NOT CALL ANY METHOD ON IT ! */
    _asyncEvents.onDisconnect([](AsyncEventSourceClient *client) {
        IPAddress ip = client->remoteIP();
        uint16_t port = client->remotePort();
        EV_TRACE_TAG_LOG("onDisconnect, port: %u, client_ID = %u", port, client->id());
        if (_eventNum) {
            --_eventNum;
        }
    });

#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
    _asyncEvents.addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
        size_t num = _asyncEvents.count();
        EV_TAG_SERIAL("onMiddle %u, %u, %u", num + 1, _eventNum + 1, NUM_EVT_CONNECTION_MAX);
#if (0)
        if ((_eventNum + 1) > NUM_EVT_CONNECTION_MAX) 
#endif
        {
            if (ticker_status(&_evConnectionInhibit) == TICKER_RUNNING) {
                EV_TRACE_TAG_LOG("onMiddle connection inhibited. Server is busy");
                request->send(503, "text/plain", "Server is busy");
                return;
            }
        } 
        // process next middleware and at the end the handler
        next();
    });
#endif

    ticker_attach(&_ws_ticker, WS_CLEAN_UP_INTERVAL, [](void *arg){
        ESPWebsocket *handler = (ESPWebsocket*)arg;
        handler->intervalCleanUpClients();
    }, this);

    connectionClean();

    ticker_begin(&_evConnectionInhibit, 1);
    ticker_begin(&_wsConnectionInhibit, 1);
    ticker_begin(&_evCloseAll, ASYNC_EV_CLOSE_ALL_TIMEOUT);
    ticker_begin(&_wsCloseAll, ASYNC_WS_CLOSE_ALL_TIMEOUT);
}

void ESPWebsocket::intervalCleanUpClients()
{
    uint8_t evt_cnt = _asyncEvents.count();
    uint8_t ws_cnt = _asyncWs.count();

    EV_TAG_SERIAL("Ev count(): %u, %u", evt_cnt, _eventNum);
    if (evt_cnt > 0) {
        if (evt_cnt > NUM_EVT_CONNECTION_MAX) {
            EV_TRACE_TAG_LOG("shall close 1/%u connection of the oldest client", evt_cnt);
            _asyncEvents.close(1); /* Close 1 connection of the oldest client */
            EV_TAG_SERIAL("connection inhibited within %u(s)", ASYNC_EV_DISCONNECT_DELAY / 1000);
            ticker_begin(&_evConnectionInhibit, ASYNC_EV_DISCONNECT_DELAY);
        }
    }

    WS_TAG_CONSOLE("count() : %u : %u, %u", ws_cnt, _asyncWs.countAll(), connectedNumber());
    if (ws_cnt > 0) {
        if (ticker_expired(&_wsCloseAll)) {
            WS_TRACE_TAG_LOG("shall close all %u, %u, %u connection due to timeout %u(s)", ws_cnt, _asyncWs.countAll(), connectedNumber(), ASYNC_WS_CLOSE_ALL_TIMEOUT / 1000);
            _asyncWs.closeAll();
            ticker_begin(&_wsConnectionInhibit, ASYNC_WS_DISCONNECT_DELAY);
            ticker_begin(&_evConnectionInhibit, ASYNC_EV_DISCONNECT_DELAY);
        }
        else {
#if (defined RS485_FRAME_MASTER_HANDLER) && (RS485_FRAME_MASTER_HANDLER == 1)
            if (pageIndexConnected(P_CARD_READER) != 0) {
                String socketNum = "{\"page\":100,\"socket_num\":" + String(ws_cnt) + "}";
                pageTypeSend(nullptr, (char*)socketNum.c_str(), P_CARD_READER);
                // _asyncWs.textAll("{\"page\":100,\"socket_num\":" + String(ws_cnt) + "}");
            }
#endif
            if (ws_cnt > NUM_WS_CONNECTION_MAX) {
                WS_TRACE_TAG_LOG("shall close 1/%u connection of the oldest client", ws_cnt);
                _asyncWs.cleanupClients(NUM_WS_CONNECTION_MAX);
                disconnectOldestOnce();
            }
        }
    } else {
        connectionClean();
        if (ticker_expired(&_evCloseAll)) {
            if (evt_cnt > 0) {
                EV_TRACE_TAG_LOG("shall close all %u, %u connection due to timeout %u(s)", evt_cnt, _eventNum, ASYNC_EV_CLOSE_ALL_TIMEOUT / 1000);
                _asyncEvents.close(); /* Close all connection */
                EV_TAG_SERIAL("connection inhibited within %u(s)", ASYNC_EV_DISCONNECT_DELAY / 1000);
                ticker_begin(&_evConnectionInhibit, ASYNC_EV_DISCONNECT_DELAY);
            }
        }
    }
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
        WS_TRACE_TAG_LOG("onConnect from %d.%d.%d.%d port: %u url: %s, client_ID = %u", ip[0], ip[1], ip[2], ip[3], port, server->url(), client->id());

        /* NUM_WS_CONNECTION_MAX < DEFAULT_MAX_WS_CLIENTS
         * Because at least have a socket connection free
         */
#if (0)
        /**
         * Now it handled on AsyncMiddlewareFunction
        */
        if (0 == connectionAvailable()) {
            disconnectOldestOnce();
        }
#endif
        ticker_attach_ms_trigger(&_ws_ticker, WS_EV_CONNECTED_CLEAN_UP_TIMEOUT);
        WS_TAG_CONSOLE("onConnect restart timeout close all clients after %u(s)", ASYNC_WS_CLOSE_ALL_TIMEOUT / 1000);
        ticker_restart(&_wsCloseAll);

        /* new connection establish */
        connectionEstablish(client->id());

        uint8_t ws_cnt = connectedNumber();
        WS_TAG_CONSOLE("onConnect connectedNumber: %u", ws_cnt);
        client->setCloseClientOnQueueFull(false);
        // client->ping();  // no needed
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        WS_TRACE_TAG_LOG("onDisconnect, client_ID = %u ", client->id());
        WS_TAG_SERIAL("remove page %s, id = %u", page_type_str[pageTypeSocket(client->id())], client->id());
        connectionRemove(client->id());
        // client->close();  // no needed: fixed in AsyncWebSocket.cpp
    }
    else if (type == WS_EVT_ERROR)
    {
        /* %.50s: Fixed crash due to the data have no terminal character */
        WS_TRACE_TAG_LOG("onErr[%s][%u] error(%u): %.50s", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
    }
    else if (type == WS_EVT_PONG)
    {
        //pong message was received (in response to a ping request maybe)
        /* %.50s: Fixed crash due to the data have no terminal character */
        WS_TAG_CONSOLE("onPong[%s][%u] pong[%u]: %.50s", server->url(), client->id(), len, (len) ? (char *)data : "");
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        String msg = "";
        if (info->final && info->index == 0 && info->len == len)
        {
            //the whole message is in a single frame and we got all of it's data
            WS_TAG_CONSOLE("[s]onData[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
            // client->ping();  // no needed
            if (info->opcode == WS_TEXT)
            {
                for (size_t i = 0; i < info->len; i++)
                {
                    msg += (char)data[i];
                }
                WS_TAG_CONSOLE("%s", msg.c_str());
                /* Call callback */
                if (_dataHandler)
                {
                    _dataHandler(client, (char *)msg.c_str());
                }

                _pCallbacks->onDataReceived(client, (char *)msg.c_str());
            }
            else
            {
                char buff[4];   /* 2024/11/11 Fix crash due to declare buff[3] */
                for (size_t i = 0; i < info->len; i++)
                {
                    snprintf(buff, 4, "%02x ", (uint8_t)data[i]);
                    msg += buff;
                }
            }
        }
        else
        {
            //message is comprised of multiple frames or the frame is split into multiple packets
            if (info->index == 0)
            {
                if (info->num == 0) {
                    WS_TAG_CONSOLE("[m]onData[%s][%u] %s-message start", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
                }
                WS_TAG_CONSOLE("[m]onData[%s][%u] frame[%u] start[%llu]", server->url(), client->id(), info->num, info->len);
            }

            WS_TAG_CONSOLE("[m]onData[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
            // client->ping();  // no needed
            if (info->message_opcode == WS_TEXT)
            {
                for (size_t i = 0; i < len; i++)
                {
                    msg += (char)data[i];
                }
            }
            else
            {
                char buff[4];   /* 2024/11/11 Fix crash due to declare buff[3] */
                for (size_t i = 0; i < len; i++)
                {
                    snprintf(buff, 4, "%02x ", (uint8_t)data[i]);
                    msg += buff;
                }
            }
            WS_TAG_CONSOLE("%s", msg.c_str());

            if ((info->index + len) == info->len)
            {
                WS_TAG_CONSOLE("[m]onData[%s][%u] frame[%u] end[%llu]", server->url(), client->id(), info->num, info->len);
                if (info->final)
                {
                    WS_TAG_CONSOLE("[m]onData[%s][%u] %s-message end", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
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
    WS_TAG_CONSOLE("Disconnect client and Remove at _ws_connection[%u].ws_num = %u", ws_index, _ws_connection[ws_index].ws_num);
    _asyncWs.close(_ws_connection[ws_index].ws_num);
    _ws_connection[ws_index].status = ASYNC_WS_DISCONNECT;
    _ws_connection[ws_index].ws_num = 0;
    _ws_connection[ws_index].pump = 0;
    _ws_connection[ws_index].page = P_NULL;
    _ws_connection[ws_index].home_seen = 0;
    _ws_connection[ws_index].timestamp = millis();
}

/* Add connection */
void ESPWebsocket::connectionEstablish(uint8_t ws_num)
{
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (ASYNC_WS_DISCONNECT == _ws_connection[i].status)
        {
            WS_TAG_CONSOLE("Added _ws_connection[%u].ws_num = %u", i, ws_num);
            _ws_connection[i].status = ASYNC_WS_CONNECTED;
            _ws_connection[i].ws_num = ws_num;
            _ws_connection[i].pump = 0;
            _ws_connection[i].page = P_NULL;
            _ws_connection[i].home_seen = 0;
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
            WS_TAG_CONSOLE("Remove _ws_connection[%u] = %u", i, ws_num);
            _ws_connection[i].status = ASYNC_WS_DISCONNECT;
            _ws_connection[i].ws_num = 0;
            _ws_connection[i].pump = 0;
            _ws_connection[i].page = P_NULL;
            _ws_connection[i].home_seen = 0;
            _ws_connection[i].timestamp = millis();
            break;
        }
    }
}
void ESPWebsocket::connectionClean()
{
    WS_TAG_CONSOLE("clean up");
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        _ws_connection[i].status = ASYNC_WS_DISCONNECT;
        _ws_connection[i].ws_num = 0;
        _ws_connection[i].pump = 0;
        _ws_connection[i].page = P_NULL;
        _ws_connection[i].home_seen = 0;
        _ws_connection[i].timestamp = millis();
    }
}

void ESPWebsocket::disconnectOldestOnce() {
    WS_TAG_CONSOLE("wS Close 1 connection of the oldest client");
    /* Disconnect socket has timelive max */
    disconnect(connectionHasTimeLiveMax());
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

    WS_TAG_CONSOLE("Available %u", count);
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

    // WS_TAG_CONSOLE("Connected %u", count);
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

    WS_TAG_CONSOLE("Max timelive at _ws_connection[%u].timestamp = %u", ws_index, tl_max / 1000);
    return ws_index;
}


/*  Brief: get socket match with page has updated
    Return: index bit
*/
uint16_t ESPWebsocket::pageIndexConnected(ws_page_connect_t Page)
{
    uint16_t index = 0;
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (clientIdIsConnected(i) && pageTypeGet(i) == (uint8_t)Page)
        {
            bitSet(index, i);
        }
    }
    return index;
}

/*  Brief: true when any connected socket is on P_HOME or has visited it (the
    detail view of home.htm flips the same socket to P_INDEX). Used by the
    history-cache release logic (B-lite). */
bool ESPWebsocket::homePageActive(void)
{
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; ++i)
    {
        if (clientIdIsConnected(i) &&
            (((uint8_t)P_HOME == _ws_connection[i].page) || _ws_connection[i].home_seen))
        {
            return true;
        }
    }
    return false;
}

/*  Brief: update page corresponding with socket */
void ESPWebsocket::pageTypeUpdate(uint8_t ws_num, ws_page_connect_t Page)
{
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; i++)
    {
        if (clientIdIsConnected(i) && clientIdGet(i) == ws_num)
        {
            _ws_connection[i].page = Page;
            if (Page == P_HOME)
            {
                /* Single-page app: the detail view moves the same socket to
                 * P_INDEX; remember that it watched home so the history cache
                 * is not released while the tab is still open. */
                _ws_connection[i].home_seen = 1;
            }
            WS_TAG_CONSOLE("_ws_connection[%u].page = %u", i, Page);
            break;
        }
    }
}

/*  Brief: update pump corresponding with socket */
void ESPWebsocket::pumpIdUpdate(uint8_t ws_num, uint8_t Pump)
{
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; i++)
    {
        if (clientIdIsConnected(i) && clientIdGet(i) == ws_num)
        {
            _ws_connection[i].pump = Pump;
            WS_TAG_CONSOLE("_ws_connection[%u].pump = %u", i, Pump);
            break;
        }
    }
}

bool ESPWebsocket::isPageType(uint8_t ws_num, ws_page_connect_t Page) {
    bool status = false;
    int8_t index = socketIndex(ws_num);
    if (index != -1) {
        status = (pageTypeGet(index) == Page);
        WS_TAG_SERIAL("%s %u", __func__, Page);
    }
    return status;
}

int8_t ESPWebsocket::socketIndex(uint8_t ws_num) {
    int8_t index = -1;
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; i++)
    {
        if (clientIdIsConnected(i) && clientIdGet(i) == ws_num)
        {
            index = i;
            WS_TAG_SERIAL("%s is %u from id %u", __func__, index, ws_num);
            break;
        }
    }
    return index;
}

uint8_t ESPWebsocket::pageTypeSocket(uint8_t ws_num) {
    uint8_t page = P_NULL;
    int8_t index = socketIndex(ws_num);
    if (index != -1) {
        page = pageTypeGet(index);
    }
    return page;
}

uint8_t ESPWebsocket::clientIdIsConnected(uint8_t index)
{
    return (ASYNC_WS_CONNECTED == _ws_connection[index].status);
}

uint8_t ESPWebsocket::pageTypeGet(uint8_t index)
{
    return _ws_connection[index].page;
}

uint8_t ESPWebsocket::clientIdGet(uint8_t index)
{
    return _ws_connection[index].ws_num;
}

uint8_t ESPWebsocket::pumpIdGet(uint8_t index)
{
    return _ws_connection[index].pump;
}

/*  Brief: send data corresponding with page */
bool ESPWebsocket::pageTypeSend(AsyncWebSocketMessageBuffer *payload, char *Data, ws_page_connect_t Page, size_t length)
{
    bool status = false;
    AsyncWebSocketSharedBuffer buffer = nullptr;
    if (payload) {
        /* Fix crash when multiple clients connected */
        buffer = std::move(payload->buffer());
        delete payload;
    }

    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; i++)
    {
        if (!clientIdIsConnected(i) || pageTypeGet(i) != (uint8_t)Page) {
            continue;
        }

        uint8_t ws_index = _ws_connection[i].ws_num;
        bool available = _asyncWs.availableForWrite(ws_index);
        if (available) {
            if (buffer != nullptr) {
                status = _asyncWs.text(ws_index, buffer);
            }
            if (Data != nullptr) {
                if (length) {
                    status = _asyncWs.text(ws_index, Data, length);
                } else {
                    status = _asyncWs.text(ws_index, Data);
                }
            }
            status = true;
        }
    }
    if (!status) {
        WS_SEND_TAG_SERIAL("%s failure 1", __func__);
    }
    return status;
}

#if (0)
bool ESPWebsocket::pageTypeSend(AsyncWebSocketSharedBuffer payload, char *Data, ws_page_connect_t Page)
{
    bool status = false;
    for (uint8_t i = 0; i < NUM_WS_CONNECTION_MAX; i++)
    {
        if (!clientIdIsConnected(i) || pageTypeGet(i) != (uint8_t)Page) {
            continue;
        }

        uint8_t ws_index = _ws_connection[i].ws_num;
        if (_asyncWs.availableForWrite(ws_index)) {
            if (payload != nullptr) {
                _asyncWs.text(ws_index, payload);
            }
            if (Data != nullptr) {
                _asyncWs.text(ws_index, Data);
            }
            status = true;
        }
    }
    return status;
}
#endif

/* send message to client */
void ESPWebsocket::sendTxt(uint8_t ws_index, char *payload)
{
    if (_asyncWs.availableForWrite(ws_index))
    {
        _asyncWs.text(ws_index, payload);
    }
}

void ESPWebsocket::sendTxt(uint8_t ws_index, AsyncWebSocketMessageBuffer *payload)
{
    if (_asyncWs.availableForWrite(ws_index)) {
        _asyncWs.text(ws_index, payload);
    } else {
        delete payload;
    }
}

/* send data to all connected clients */
void ESPWebsocket::sendBroadcastTxt(char *payload)
{
    if (_asyncWs.availableForWriteAll())
    {
        _asyncWs.textAll(payload);
    }
}

void ESPWebsocket::sendBroadcastTxt(AsyncWebSocketMessageBuffer *payload)
{
    if (_asyncWs.availableForWriteAll()) {
        _asyncWs.textAll(payload);
    } else {
        delete payload;
    }
}

WebsocketCallbacks::~WebsocketCallbacks() {}
/**
* Handler called after once received data.
*/
void WebsocketCallbacks::onDataReceived(AsyncWebSocketClient* client, char* data)
{
    WS_TAG_CONSOLE("[WebsocketCallbacks] >> onDataReceived: default <<");
}

ESPWebsocket wsHandler;