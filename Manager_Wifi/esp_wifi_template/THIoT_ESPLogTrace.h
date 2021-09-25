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
public:
    ESPLogTrace(fs::FS &fs = NAND_FS_SYSTEM, const char* fileName = "/LOGTRACE.TXT");
    ~ESPLogTrace();

    int available() override { return 0;}
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush(void) override {}
    size_t write(uint8_t c) override { write(&c, 1); return 1; }
    size_t write(const uint8_t *buffer, size_t size) override;
};

extern ESPLogTrace ESPLOG;

#endif // __ESP_LOG_TRACE_H