#ifndef __ESP_LOG_TRACE_H
#define __ESP_LOG_TRACE_H

#include <Arduino.h>
#include <FS.h>
#include "THIoT_ESPConfig.h"

class ESPLogTrace : public Stream
{
private:
    fs::FS* _fs;
    String _fileName;
    size_t _limitSize;
    bool _enable;
    void assertOverSize(size_t limit);
    String backupName() {
        int dotIndex = _fileName.indexOf(".");
        String name = _fileName.substring(0, dotIndex);
        name += "_BK.TXT";
        return name;
    }
public:
    ESPLogTrace(fs::FS &fs = NAND_FS_SYSTEM, const char* fileName = "/LOGTRACE.TXT", size_t limitSize = (200 * 1024));
    ~ESPLogTrace();

    int available() override { return 0;}
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush(void) override {}
    size_t write(uint8_t c) override { write(&c, 1); return 1; }
    size_t write(const uint8_t *buffer, size_t size) override;
    void clean() {
        _fs->remove(backupName());
        _fs->remove(_fileName);
    }
    void enable() { _enable = true; }
    void disable() { _enable = false; }
};

extern ESPLogTrace ESPLOG;

#define FS_PORT ESPLOG
#define FS_LOG_CLEAN() FS_PORT.clean()
#define FS_FLUSH() FS_PORT.flush()
#define FS_PRINTF(f_, ...)  FS_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)
#define FS_PRINTFLF(f_, ...)  FS_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)
#define FS_TAG_PRINTF(tag, f_, ...)  FS_PORT.printf_P(PSTR(tag f_), ##__VA_ARGS__)
#define FS_FUNCTION_PRINTF(f_, ...)  FS_PORT.printf_P(PSTR(f_), __FUNCTION__, ##__VA_ARGS__)
#define FS_FUNCTION_PRINTFLF(f_, ...)  FS_PORT.printf_P(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)

#define g_fsLevel 4

#define FS_LOGE(...) do{if(g_fsLevel > 0) {FS_PRINTFLF("E: " __VA_ARGS__);}}while(0)
#define FS_LOGW(...) do{if(g_fsLevel > 1) {FS_PRINTFLF("W: " __VA_ARGS__);}}while(0)
#define FS_LOGI(...) do{if(g_fsLevel > 2) {FS_PRINTFLF("I: " __VA_ARGS__);}}while(0)
#define FS_LOGD(...) do{if(g_fsLevel > 3) {FS_PRINTFLF("D: " __VA_ARGS__);}}while(0)
#define FS_LOGV(...) do{if(g_fsLevel > 4) {FS_PRINTFLF("V: " __VA_ARGS__);}}while(0)

#define FS_TAG_LOGE(x, ...) do{if(g_fsLevel > 0) {FS_PRINTFLF("E " x ": " __VA_ARGS__);}}while(0)
#define FS_TAG_LOGW(x, ...) do{if(g_fsLevel > 1) {FS_PRINTFLF("W " x ": " __VA_ARGS__);}}while(0)
#define FS_TAG_LOGI(x, ...) do{if(g_fsLevel > 2) {FS_PRINTFLF("I " x ": " __VA_ARGS__);}}while(0)
#define FS_TAG_LOGD(x, ...) do{if(g_fsLevel > 3) {FS_PRINTFLF("D " x ": " __VA_ARGS__);}}while(0)
#define FS_TAG_LOGV(x, ...) do{if(g_fsLevel > 4) {FS_PRINTFLF("V " x ": " __VA_ARGS__);}}while(0)

#if (g_fsLevel > 0)
#define FS_FUNCTION_IN(...) do{FS_FUNCTION_PRINTFLF("FUNCTION [%s] IN " __VA_ARGS__);}while(0)
#define FS_FUNCTION_OUT(...) do{FS_FUNCTION_PRINTFLF("FUNCTION [%s] OUT " __VA_ARGS__);}while(0)
#define FS_FUNCTION_LOGI(...) do{FS_FUNCTION_PRINTFLF("I: FUNCTION [%s] " __VA_ARGS__);}while(0)
#define FS_FUNCTION_TAG_LOGI(x, ...) do{FS_FUNCTION_PRINTFLF("I " x ": FUNCTION [%s] " __VA_ARGS__);}while(0)
#else
#define FS_FUNCTION_IN(...)
#define FS_FUNCTION_OUT(...)
#define FS_FUNCTION_LOGI(...)
#define FS_FUNCTION_TAG_LOGI(x, ...)
#endif

#endif // __ESP_LOG_TRACE_H