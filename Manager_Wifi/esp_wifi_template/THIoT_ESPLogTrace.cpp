#include "THIoT_ESPConfig.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_SerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define LOG_TRACE_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[LOG_TRACE]", __VA_ARGS__)
#define LOG_TRACE_FUNCTION_CONSOLE(...) SERIAL_FUNCTION_TAG_LOGI("[LOG_TRACE]", __VA_ARGS__)

ESPLogTrace::ESPLogTrace(fs::FS &fs, const char* fileName, size_t limitSize)
:_fs(&fs),_fileName(fileName), _limitSize(limitSize) {
    _enable = true;
}

ESPLogTrace::~ESPLogTrace() {
}

void ESPLogTrace::assertOverSize(size_t limit) {
    File fs_handle = _fs->open(_fileName, "r");
    size_t size = fs_handle.size();
    fs_handle.close();
    if (size >= _limitSize) {
        LOG_TRACE_FUNCTION_CONSOLE("File size = %lu, limit size = %lu", size, _limitSize);
        LOG_TRACE_FUNCTION_CONSOLE("backupName is %s", backupName().c_str());
        _fs->remove(backupName());
        _fs->rename(_fileName, backupName());
    }
}

// Should be have return size value to prevent crash.
size_t ESPLogTrace::write(const uint8_t *buffer, size_t size) {
    if (_enable)
    {
        assertOverSize(_limitSize);
        String time = ESPTime.toStringLog();
        File fs_handle = _fs->open(_fileName, "a");
        fs_handle.print(time);
        fs_handle.write(buffer, size);
        // fs_handle.println();
        fs_handle.close();
        LOG_TRACE_FUNCTION_CONSOLE("succeed!");
    }
    return size;
}

ESPLogTrace ESPLOG(NAND_FS_SYSTEM);
