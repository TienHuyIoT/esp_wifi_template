#include "THIoT_ESPConfig.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_SerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define LOG_TRACE_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[LOG_TRACE]", __VA_ARGS__)
#define LOG_TRACE_FUNCTION_CONSOLE(...) FUNCTION_TAG_LOGI("[LOG_TRACE]", __VA_ARGS__)

ESPLogTrace::ESPLogTrace(fs::FS &fs, const char* fileName)
:_fs(&fs),_fileName(fileName) {
}

ESPLogTrace::~ESPLogTrace() {
}

// Should be have return size value to prevent crash.
size_t ESPLogTrace::write(const uint8_t *buffer, size_t size) {
    String time = ESPTime.toStringLog();
    File fs_handle = _fs->open(_fileName, "a");
    fs_handle.print(time);
    fs_handle.write(buffer, size);
    fs_handle.println();
    fs_handle.close();
    LOG_TRACE_FUNCTION_CONSOLE("succeed!");
    return size;
}

ESPLogTrace ESPLOG(NAND_FS_SYSTEM);
