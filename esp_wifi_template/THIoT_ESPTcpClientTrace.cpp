#include "THIoT_ESPConfig.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPTcpClientTrace.h"

#if (TCP_CLIENT_TRACE_START_ON_WIFI_EVENT == 1)
#if (TCP_CLIENT_TRACE_DEBUG == 1)
#include "THIoT_PFSerialTrace.h"
#define TCP_CLIENT_PORT_CONSOLE                 SERIAL_PORT
#define TCP_CLIENT_TRACE_TAG_CONSOLE(...)       CONSOLE_TAG_LOGI("[TCP_TRACE]", __VA_ARGS__)
#define TCP_CLIENT_TRACE_FUNCTION_CONSOLE(...)  CONSOLE_FUNCTION_TAG_LOGI("[TCP_TRACE]", __VA_ARGS__)
#else
#define TCP_CLIENT_PORT_CONSOLE           
#define TCP_CLIENT_TRACE_TAG_CONSOLE(...) 
#define TCP_CLIENT_TRACE_FUNCTION_CONSOLE(...)
#endif

#define DEFAULT_RX_TIMEOUT                      120

#define _seize xSemaphoreTakeRecursive(_threadLock,portMAX_DELAY)
#define _release xSemaphoreGiveRecursive(_threadLock)

ESPTcpClientTrace::ESPTcpClientTrace()
:_request(nullptr), _client(nullptr), _timeout(DEFAULT_RX_TIMEOUT)
{
    _enable = false;
    _threadLock = xSemaphoreCreateRecursiveMutex();
}

ESPTcpClientTrace::~ESPTcpClientTrace() {
    if( ! _client){
        delete _client;
    }
    if( ! _request) {
        delete _request;
    }
}

void ESPTcpClientTrace::begin(const char* host, int port) {
    if (_enable) {
        return;
    }
    
    if( ! _client){
        _client = new AsyncClient();
    }

    if( ! _request) {
        _request = new xbuf;
    }

    if( ! _response) {
        _response = new xbuf;
    }

    _host = host;
    _port = port;
    _connect();
}

// Should be have return size value to prevent crash.
size_t ESPTcpClientTrace::write(const uint8_t *buffer, size_t size) {
    if (!_enable) {
        return 0;
    }
    _seize;
#if (TCP_CLIENT_TRACE_DEBUG == 1)
    TCP_CLIENT_PORT_CONSOLE.write(buffer, size);
#endif
    _request->write(buffer, size);
    _send();
    _release;
    return size;
}

bool  ESPTcpClientTrace::_connect() {
    TCP_CLIENT_TRACE_TAG_CONSOLE("_connect()\r\n");
    if( ! _client){
        _client = new AsyncClient();
    }
    _client->onConnect([](void *obj, AsyncClient *client){((ESPTcpClientTrace*)(obj))->_onConnect(client);}, this);
    _client->onDisconnect([](void *obj, AsyncClient* client){((ESPTcpClientTrace*)(obj))->_onDisconnect(client);}, this);
    _client->onPoll([](void *obj, AsyncClient *client){((ESPTcpClientTrace*)(obj))->_onPoll(client);}, this);
    _client->onError([](void *obj, AsyncClient *client, uint32_t error){((ESPTcpClientTrace*)(obj))->_onError(client, error);}, this);
    if( ! _client->connected()){
        if( ! _client->connect(_host.c_str(), _port)) {
            TCP_CLIENT_TRACE_TAG_CONSOLE("!client.connect(%s, %d) failed\r\n", _host.c_str(), _port);
            return false;
        }
    }
    else {
        _onConnect(_client);
    }
    return true;
}

size_t  ESPTcpClientTrace::_send() {
    if( ! _request) return 0;
    TCP_CLIENT_TRACE_TAG_CONSOLE("_send() %d\r\n", _request->available());
    if( ! _client->connected() || ! _client->canSend()){
        TCP_CLIENT_TRACE_TAG_CONSOLE("*can't send\r\n");
        return 0;
    }
    size_t supply = _request->available();
    size_t demand = _client->space();
    if(supply > demand) supply = demand;
    size_t sent = 0;
    uint8_t* temp = new uint8_t[100];
    while(supply){
        size_t chunk = supply < 100 ? supply : 100;
        supply -= _request->read(temp, chunk);
        sent += _client->add((char*)temp, chunk);
    }
    delete temp;

    _client->send();
    TCP_CLIENT_TRACE_TAG_CONSOLE("*sent %d\r\n", sent);
    _lastActivity = millis();
    return sent;
}


/*______________________________________________________________________________________________________________

EEEEE   V   V   EEEEE   N   N   TTTTT         H   H    AAA    N   N   DDDD    L       EEEEE   RRRR     SSS
E       V   V   E       NN  N     T           H   H   A   A   NN  N   D   D   L       E       R   R   S
EEE     V   V   EEE     N N N     T           HHHHH   AAAAA   N N N   D   D   L       EEE     RRRR     SSS
E        V V    E       N  NN     T           H   H   A   A   N  NN   D   D   L       E       R  R        S
EEEEE     V     EEEEE   N   N     T           H   H   A   A   N   N   DDDD    LLLLL   EEEEE   R   R    SSS
_______________________________________________________________________________________________________________*/

//**************************************************************************************************************
void  ESPTcpClientTrace::_onConnect(AsyncClient* client){
    TCP_CLIENT_TRACE_TAG_CONSOLE("_onConnect handler\r\n");
    _seize;
    _client = client;
    _client->onAck([](void* obj, AsyncClient* client, size_t len, uint32_t time){((ESPTcpClientTrace*)(obj))->_send();}, this);
    _client->onData([](void* obj, AsyncClient* client, void* data, size_t len){((ESPTcpClientTrace*)(obj))->_onData(data, len);}, this);
    _lastActivity = millis();
    _enable = true;
    _release;
}

//**************************************************************************************************************
void  ESPTcpClientTrace::_onPoll(AsyncClient* client){
    _seize;
    if(_timeout && (millis() - _lastActivity) > (_timeout * 1000)){
        _client->close();
        TCP_CLIENT_TRACE_TAG_CONSOLE("_onPoll timeout\r\n");
    }
    _release;
}

//**************************************************************************************************************
void  ESPTcpClientTrace::_onError(AsyncClient* client, int8_t error){
    TCP_CLIENT_TRACE_TAG_CONSOLE("_onError handler error=%d\r\n", error);
}

//**************************************************************************************************************
void  ESPTcpClientTrace::_onDisconnect(AsyncClient* client){
    TCP_CLIENT_TRACE_TAG_CONSOLE("_onDisconnect handler\r\n");
    _seize;
    delete _client;
    _client = nullptr;
    _lastActivity = 0;
    _enable = false;
    _release;
}

//**************************************************************************************************************
void  ESPTcpClientTrace::_onData(void* Vbuf, size_t len){
    TCP_CLIENT_TRACE_TAG_CONSOLE("_onData handler %.16s... (%d)\r\n",(char*) Vbuf, len);
    _seize;
    _lastActivity = millis();
    // _response->write((uint8_t*)Vbuf, len);
    _release;

}

ESPTcpClientTrace TCPTrace;
#endif // #if (TCP_CLIENT_TRACE_START_ON_WIFI_EVENT == 1)
