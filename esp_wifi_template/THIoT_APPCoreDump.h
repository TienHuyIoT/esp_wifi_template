#ifndef __THIOT_APPCORE_DUMP_H__
#define __THIOT_APPCORE_DUMP_H__

#include <Arduino.h>
#include <sdkconfig.h>
#include "THIoT_APPConfig.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPSDFSClass.h"

#define CORE_DUMP_FS_LOG_SYSTEM SD_FS_SYSTEM
#define CORE_DUMP_RECORD_PATH ((const char *)"/CORE_DUMP.bin")

#if defined(CORE_DUMP_TRACE_RECORD) && (CORE_DUMP_TRACE_RECORD == 1)
class THIoTAppCoreDump
{
    using File = fs::File;
    using FS = fs::FS;
private:
    FS *_fs;
    String _fileName;

public:
    THIoTAppCoreDump(FS &fs = CORE_DUMP_FS_LOG_SYSTEM, const char *fileName = CORE_DUMP_RECORD_PATH);
    ~THIoTAppCoreDump();
    bool record();
};

extern THIoTAppCoreDump gAppCoreDump;
#endif // defined(CORE_DUMP_TRACE_RECORD) && (CORE_DUMP_TRACE_RECORD == 1)

#endif // __THIOT_APPCORE_DUMP_H__