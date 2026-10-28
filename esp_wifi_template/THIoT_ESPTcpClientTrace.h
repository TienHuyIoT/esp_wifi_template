#ifndef __ESP_TCP_CLIENT_TRACE_H
#define __ESP_TCP_CLIENT_TRACE_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include "THIoT_APPConfig.h"

#if (TCP_CLIENT_TRACE_START_ON_WIFI_EVENT == 1)
#include <pgmspace.h>
#include <xbuf.h>
#include "THIoT_ESPConfig.h"

#define TCP_CLIENT_TRACE_DEBUG          0
/**
 * Only enable for debug over TCP
*/
#define TCP_CLIENT_HOST_CONSOLE         "192.168.1.13"
#define TCP_CLIENT_PORT_CONSOLE         25130

class ESPTcpClientTrace : public Stream
{
private:
    AsyncClient*        _client;
    xbuf*               _request;
	xbuf*               _response;
    SemaphoreHandle_t   _threadLock;
    String _host;
    int _port;
    uint32_t            _timeout;
    uint32_t            _lastActivity;
    bool                _enable;
    size_t              _send();
    bool                _connect();

    void                _onConnect(AsyncClient*);
    void                _onDisconnect(AsyncClient*);
    void                _onData(void*, size_t);
    void                _onError(AsyncClient*, int8_t);
    void                _onPoll(AsyncClient*);
public:
    ESPTcpClientTrace();
    ~ESPTcpClientTrace();

    void begin(const char* host, int port);
    int available() override { return 0;}
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush(void) override {}
    size_t write(uint8_t c) override { write(&c, 1); return 1; }
    size_t write(const uint8_t *buffer, size_t size) override;
    void enable() { _enable = true; }
    void disable() { _enable = false; }
};

extern ESPTcpClientTrace TCPTrace;

#define TCP_PORT TCPTrace
#define TCP_LOG_CLEAN() TCP_PORT.clean()
#define TCP_FLUSH() TCP_PORT.flush()
#define TCP_PRINTF(f_, ...)  TCP_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)
#define TCP_PRINTFLF(f_, ...)  TCP_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)
#define TCP_TAG_PRINTF(tag, f_, ...)  TCP_PORT.printf_P(PSTR(tag f_), ##__VA_ARGS__)
#define TCP_FUNCTION_PRINTF(f_, ...)  TCP_PORT.printf_P(PSTR(f_), __FUNCTION__, ##__VA_ARGS__)
#define TCP_FUNCTION_PRINTFLF(f_, ...)  TCP_PORT.printf_P(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)

#define gTcpDebugLevel 4

#define TCP_LOGE(...) do{if(gTcpDebugLevel > 0) {TCP_PRINTFLF("E: " __VA_ARGS__);}}while(0)
#define TCP_LOGW(...) do{if(gTcpDebugLevel > 1) {TCP_PRINTFLF("W: " __VA_ARGS__);}}while(0)
#define TCP_LOGI(...) do{if(gTcpDebugLevel > 2) {TCP_PRINTFLF("I: " __VA_ARGS__);}}while(0)
#define TCP_LOGD(...) do{if(gTcpDebugLevel > 3) {TCP_PRINTFLF("D: " __VA_ARGS__);}}while(0)
#define TCP_LOGV(...) do{if(gTcpDebugLevel > 4) {TCP_PRINTFLF("V: " __VA_ARGS__);}}while(0)

#define TCP_TAG_LOGE(x, ...) do{if(gTcpDebugLevel > 0) {TCP_PRINTFLF("E " x ": " __VA_ARGS__);}}while(0)
#define TCP_TAG_LOGW(x, ...) do{if(gTcpDebugLevel > 1) {TCP_PRINTFLF("W " x ": " __VA_ARGS__);}}while(0)
#define TCP_TAG_LOGI(x, ...) do{if(gTcpDebugLevel > 2) {TCP_PRINTFLF("I " x ": " __VA_ARGS__);}}while(0)
#define TCP_TAG_LOGD(x, ...) do{if(gTcpDebugLevel > 3) {TCP_PRINTFLF("D " x ": " __VA_ARGS__);}}while(0)
#define TCP_TAG_LOGV(x, ...) do{if(gTcpDebugLevel > 4) {TCP_PRINTFLF("V " x ": " __VA_ARGS__);}}while(0)

#define TCP_FUNCTION_IN(...) do{TCP_FUNCTION_PRINTFLF("FUNCTION [%s] IN " __VA_ARGS__);}while(0)
#define TCP_FUNCTION_OUT(...) do{TCP_FUNCTION_PRINTFLF("FUNCTION [%s] OUT " __VA_ARGS__);}while(0)
#define TCP_FUNCTION_LOGI(...) do{TCP_FUNCTION_PRINTFLF("I: FUNCTION [%s] " __VA_ARGS__);}while(0)
#define TCP_FUNCTION_TAG_LOGI(x, ...) do{TCP_FUNCTION_PRINTFLF("I " x ": FUNCTION [%s] " __VA_ARGS__);}while(0)

#else

#define TCP_LOG_CLEAN()
#define TCP_FLUSH()
#define TCP_PRINTF(f_, ...)
#define TCP_PRINTFLF(f_, ...)
#define TCP_TAG_PRINTF(tag, f_, ...)  
#define TCP_FUNCTION_PRINTF(f_, ...)  
#define TCP_FUNCTION_PRINTFLF(f_, ...)

#define TCP_LOGE(...)
#define TCP_LOGW(...)
#define TCP_LOGI(...)
#define TCP_LOGD(...)
#define TCP_LOGV(...)

#define TCP_TAG_LOGE(x, ...)
#define TCP_TAG_LOGW(x, ...)
#define TCP_TAG_LOGI(x, ...)
#define TCP_TAG_LOGD(x, ...)
#define TCP_TAG_LOGV(x, ...)

#define TCP_FUNCTION_IN(...)
#define TCP_FUNCTION_OUT(...)
#define TCP_FUNCTION_LOGI(...)
#define TCP_FUNCTION_TAG_LOGI(x, ...)

#endif // #if (TCP_CLIENT_TRACE_START_ON_WIFI_EVENT == 1)

#endif // __ESP_TCP_CLIENT_TRACE_H