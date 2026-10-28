#ifndef	__ESP_FS_HANDLE_H
#define __ESP_FS_HANDLE_H

#include <Arduino.h>
#include <FS.h>
#include "THIoT_ESPSDFSClass.h"
#include "THIoT_ESPMessageList.h"

#define FS_HANDLE_SD_FREE_SPACE_SIZE_LIMIT      (1024ULL * 1024ULL * 512ULL) // 512MByte
#define FS_HANDLE_SD_FILE_SIZE_LIMIT            (1024ULL * 1024ULL * 32ULL)  // 32MByte

#define FS_HANDLE_NAND_FREE_SPACE_SIZE_LIMIT    (1024U * 200U)               // 200KByte
#define FS_HANDLE_NAND_FILE_SIZE_LIMIT          (1024U * 200U)               // 200KByte

class ESPFSHandle
{
    using File = fs::File;
    using FS = fs::FS;
private:
    bool isInhibited(FS *fs, String path);
public:
    ESPFSHandle(/* args */);
    ~ESPFSHandle();
    void listDir(FS &fs,
                 const char *dirname,
                 uint8_t levels = 0,
                 ESPMessageList *mList = NULL,
                 uint64_t sLimit = FS_HANDLE_SD_FILE_SIZE_LIMIT);
    boolean backup2Folder(FS &fs, String path);
    bool copy(FS *fs_src, String path_src, String path_des, FS *fs_des);
    bool cleanUp(FS &fs, uint64_t sLimit = FS_HANDLE_SD_FILE_SIZE_LIMIT);
    bool deleteRecursive(FS *fs, String path);
};

extern ESPFSHandle FSHandle;

#endif // __ESP_FS_HANDLE_H
