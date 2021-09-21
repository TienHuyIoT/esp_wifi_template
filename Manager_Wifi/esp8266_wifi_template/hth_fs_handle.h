#ifndef	_FS_HANDLE_H
#define _FS_HANDLE_H

#include <Arduino.h>
#include <FS.h>

class ESPFSHandler
{
private:
    /* data */
public:
    ESPFSHandler(/* args */);
    ~ESPFSHandler();

    void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
    void format(fs::FS &fs, const char *dirname);
    void deleteFile(fs::FS &fs, const char *path);
    void removeDir(fs::FS &fs, const char *path);
    #if (0)
    void createDir(fs::FS &fs, const char *path);
    void removeDir(fs::FS &fs, const char *path);
    void readFile(fs::FS &fs, const char *path);
    void writeFile(fs::FS &fs, const char *path, const char *message);
    void appendFile(fs::FS &fs, const char *path, const char *message);
    void renameFile(fs::FS &fs, const char *path1, const char *path2);
    void testFileIO(fs::FS &fs, const char *path);
    #endif // (0)
};

extern ESPFSHandler FSHandle;

#endif // _FS_HANDLE_H
