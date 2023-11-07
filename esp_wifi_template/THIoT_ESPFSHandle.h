#ifndef	__ESP_FS_HANDLE_H
#define __ESP_FS_HANDLE_H

#include <Arduino.h>
#include <FS.h>
#include "THIoT_ESPSDFSClass.h"

class ESPFSHandle
{
    using File = fs::File;
    using FS = fs::FS;
private:
    /* data */
public:
    ESPFSHandle(/* args */);
    ~ESPFSHandle();

    void listDir(FS &fs, const char *dirname, uint8_t levels);
    void format(FS &fs, const char *dirname);
    void deleteFile(FS &fs, const char *path);
    void removeDir(FS &fs, const char *path);
    #if (0)
    void createDir(FS &fs, const char *path);
    void removeDir(FS &fs, const char *path);
    void readFile(FS &fs, const char *path);
    void writeFile(FS &fs, const char *path, const char *message);
    void appendFile(FS &fs, const char *path, const char *message);
    void renameFile(FS &fs, const char *path1, const char *path2);
    void testFileIO(FS &fs, const char *path);
    #endif // (0)
};

extern ESPFSHandle FSHandle;

#endif // __ESP_FS_HANDLE_H
