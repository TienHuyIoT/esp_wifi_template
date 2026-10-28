#ifndef __CONSOLE_TRACE_H
#define __CONSOLE_TRACE_H

#include <Arduino.h>
#include "AsyncUDP.h"
#include <FS.h>
#include "THIoT_APPConfig.h"

#if (CONSOLE_TRACE_TASK_ENABLE == 1)
#define gConsoleDebugLevel                      4   /* 0: OFF; 4:ON */

#if (gConsoleDebugLevel > 0)
#define CONSOLE_TRACE_SERIAL_STREAM             1
#define CONSOLE_TRACE_FILE_STREAM               1
#define CONSOLE_TRACE_UDP_STREAM                0
#define CONSOLE_TRACE_WS_STREAM                 1
#define CONSOLE_EXTERNAL_MEMORY_SIZE_LIMITED    (1024 * 1024)    /* Size limit each record file */

#define CONSOLE_TRACE_FILE_PATH                 ((const char*)"/DEBUG_TRACE.TXT")

typedef std::function<bool(void *arg, const uint8_t *buffer, size_t length)> consoleHandler;

void consoleTraceTaskBegin();
void consoleTraceTaskEnd();

class ESPConsoleTrace : public Print
{
    using Print::write;
    using File = fs::File;
    using FS = fs::FS;
private:
    /* Protects the stream pointers and the enable flags shared by every task using
     * this object (console task, AsyncUDP callback, SD error handler). Held across
     * _addLog() so end()/consoleFileStop() cannot free a handle while it is written.
     * Lock order: state mutex -> file lock; a stream write is never started while the
     * state mutex is held by a *producer* (write() only snapshots the flag). */
    SemaphoreHandle_t _stateMutex;
    bool _stateLock();
    void _stateUnlock();
#if (CONSOLE_TRACE_WS_STREAM == 1)
    consoleHandler _consoleWs;
    void *_wsArg;
    TaskHandle_t _wsInFlight;   /* task currently running the WebSocket callback */
    void _waitWsIdle();
#endif
#if (CONSOLE_TRACE_FILE_STREAM == 1)
    FS* _fs;
    File *_fs_handle;
    Stream *_consoleFile;
    String _fileName;
    size_t _limitSize;
    bool _disableFileWrite;
#endif
#if (CONSOLE_TRACE_UDP_STREAM == 1)
    AsyncUDP _udp_handle;
    AsyncUDPPacket *_udpPack;
    Stream *_consoleUdp;
#endif
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    Stream *_consoleSerial;
#endif
    bool _enable;
    bool _wsEnable;
    bool _timeLogOn;
#if (CONSOLE_TRACE_FILE_STREAM == 1)
    void assertOverSize();
#endif
    size_t _addLog(const uint8_t *buffer, size_t length);
public:
    ESPConsoleTrace(Stream *console = nullptr, bool timeLogOn = true);
    ~ESPConsoleTrace();
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    void beginSerial(Stream *console);
#endif
#if (CONSOLE_TRACE_UDP_STREAM == 1)
    void beginUdp(int port);
#endif
#if (CONSOLE_TRACE_FILE_STREAM == 1)
    bool beginFs(FS &fs, const char* fileName, size_t limitSize = CONSOLE_EXTERNAL_MEMORY_SIZE_LIMITED);
#endif
#if (CONSOLE_TRACE_WS_STREAM == 1)
    void beginWs(consoleHandler cb, void* arg) {
        _stateLock();
        if (_wsEnable) {
            _consoleWs = cb;
            _wsArg = arg;
            _enable = true;
        }
        _stateUnlock();
    }
    void closeWs();
    void wsConsoleSet(bool wsEnable);
#endif
    void end();
    void flush();
    /* Brief: Hold/release the state of this object when a caller needs several state
     *        operations to be seen as one. Do not wrap a stream/file write with it. */
    void enterCritical() { _stateLock(); }
    void exitCritical() { _stateUnlock(); }
    size_t write(uint8_t c) override { write(&c, 1); return 1; }
    size_t write(const uint8_t *buffer, size_t size) override;
    void enable() { _stateLock(); _enable = true; _stateUnlock(); }
    void disable() { _stateLock(); _enable = false; _stateUnlock(); }
    bool isEnable();
#if (CONSOLE_TRACE_SERIAL_STREAM == 1)
    void consoleSerialStop();
#endif
#if (CONSOLE_TRACE_FILE_STREAM == 1)
    void consoleFileStop();
#endif
#if (CONSOLE_TRACE_UDP_STREAM == 1)
    void consoleUdpStop();
#endif
    static size_t _s_eventDataHandle(void *arg, const uint8_t *log, size_t length);
};

extern ESPConsoleTrace CONSOLE_TRACE_STREAM;

#define CONSOLE_PORT CONSOLE_TRACE_STREAM
#define CONSOLE_FLUSH() (CONSOLE_PORT.flush())
#define CONSOLE_PRINTF(f_, ...) CONSOLE_PORT.printf_P(PSTR("\xff" f_), ##__VA_ARGS__)   // disable time console
#define CONSOLE_PRINTFLN(f_, ...) CONSOLE_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)
#define CONSOLE_TAG_PRINTF(tag, f_, ...) CONSOLE_PORT.printf_P(PSTR(tag f_), ##__VA_ARGS__)
#define CONSOLE_FUNCTION_PRINTFLF(f_, ...) CONSOLE_PORT.printf_P(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)

#define CONSOLE_LOGE(...) do{if(gConsoleDebugLevel > 0) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGW(...) do{if(gConsoleDebugLevel > 1) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGI(...) do{if(gConsoleDebugLevel > 2) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGD(...) do{if(gConsoleDebugLevel > 3) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGV(...) do{if(gConsoleDebugLevel > 4) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)

#define CONSOLE_TAG_LOGE(x, ...) do{if(gConsoleDebugLevel > 0) {CONSOLE_PRINTFLN("E " x ": " __VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGW(x, ...) do{if(gConsoleDebugLevel > 1) {CONSOLE_PRINTFLN("W " x ": " __VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGI(x, ...) do{if(gConsoleDebugLevel > 2) {CONSOLE_PRINTFLN("I " x ": " __VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGD(x, ...) do{if(gConsoleDebugLevel > 3) {CONSOLE_PRINTFLN("D " x ": " __VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGV(x, ...) do{if(gConsoleDebugLevel > 4) {CONSOLE_PRINTFLN("V " x ": " __VA_ARGS__);}}while(0)

#define CONSOLE_FUNCTION_IN(...) do{CONSOLE_FUNCTION_PRINTFLF("\r\n\r\nFUNCTION [%s] IN " __VA_ARGS__);}while(0)
#define CONSOLE_FUNCTION_OUT(...) do{CONSOLE_FUNCTION_PRINTFLF("\r\nFUNCTION [%s] OUT " __VA_ARGS__);}while(0)
#define CONSOLE_FUNCTION_LOGI(...) do{CONSOLE_FUNCTION_PRINTFLF("\r\n\r\nI: FUNCTION [%s] " __VA_ARGS__);}while(0)
#define CONSOLE_FUNCTION_TAG_LOGI(x, ...) do{CONSOLE_FUNCTION_PRINTFLF("I " x ": FUNCTION [%s] " __VA_ARGS__);}while(0)
#else
#define CONSOLE_TRACE_SERIAL_STREAM             0
#define CONSOLE_TRACE_FILE_STREAM               0
#define CONSOLE_TRACE_UDP_STREAM                0

#define CONSOLE_FLUSH()

#define CONSOLE_LOGE(...) 
#define CONSOLE_LOGW(...) 
#define CONSOLE_LOGI(...) 
#define CONSOLE_LOGD(...) 
#define CONSOLE_LOGV(...) 

#define CONSOLE_TAG_LOGE(x, ...)
#define CONSOLE_TAG_LOGW(x, ...)
#define CONSOLE_TAG_LOGI(x, ...)
#define CONSOLE_TAG_LOGD(x, ...)
#define CONSOLE_TAG_LOGV(x, ...)

#define CONSOLE_FUNCTION_IN(...)
#define CONSOLE_FUNCTION_OUT(...)
#define CONSOLE_FUNCTION_LOGI(...)
#define CONSOLE_FUNCTION_TAG_LOGI(x, ...)
#endif // #if (gConsoleDebugLevel > 0)
#else
#define gConsoleDebugLevel                      0   /* 0: OFF; 4:ON */
#define CONSOLE_TRACE_SERIAL_STREAM             0
#define CONSOLE_TRACE_FILE_STREAM               0
#define CONSOLE_TRACE_UDP_STREAM                0
#define CONSOLE_TRACE_WS_STREAM                 0
#endif // #if (CONSOLE_TRACE_TASK_ENABLE == 1)

#endif // __CONSOLE_TRACE_H
