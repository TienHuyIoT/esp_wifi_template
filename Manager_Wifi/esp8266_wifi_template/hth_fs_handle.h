#ifndef	_FS_HANDLE_H
#define _FS_HANDLE_H

#include <Arduino.h>
#include <FS.h>

class hth_fs_handle
{
private:
    /* data */
public:
    hth_fs_handle(/* args */);
    ~hth_fs_handle();

    void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
    void sd_format(fs::FS &fs, const char *dirname);
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

extern hth_fs_handle HTH_fsHandle;

#endif // _FS_HANDLE_H
