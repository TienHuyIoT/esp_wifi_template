#ifndef __ESP_FS_EDITOR_H
#define __ESP_FS_EDITOR_H

/*
Usage ESP32 Sketch Data Upload
1. Download esp32fs.zip
  - link: https://github.com/lorol/arduino-esp32fs-plugin/releases
  - Unpack the tool into "C:\Users\HUY-GAMING\Documents\Arduino\tools\ESP32FS\tool\esp32fs.jar"

2. Download x86_64-w64-mingw32-mklittlefs-295fe9b.zip
  - Link: https://github.com/earlephilhower/mklittlefs/releases
  - Unpack the file into C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mklittlefs\mklittlefs.exe

3. Download mkfatfs_v1.0.exe
  - Link: https://github.com/labplus-cn/mkfatfs/releases/tag/v1.0
  - Unpack the file into C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mkfatfs\mkfatfs.exe

4. mkspiffs.exe is already in directory
  - C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mkspiffs\mkspiffs.exe

5. Install LITTLEFS
  - Link: https://github.com/lorol/LITTLEFS
  - Or Manage Libraries with search "LITTLEFS"

6. Upload FS
  - Restart Arduino IDE after any steps above
  - Tools --> ESP32 Sketch Data Upload --> [LITTLEFS, SPIFFS, FATFS, Erase Flash]

Request to download file from sd card
method get
IP/edit_sdfs?download=/YLTD.MP3

Request to edit file from sd card
method get
IP/edit_sdfs?edit=/YLTD.MP3

Request to delect file from sd card
method delect
IP/edit_sdfs , path=/YLTD.MP3

Request to download file from spiffs
method get
IP/edit?download=/log_report.csv

Request to edit file from spiffs
method get
IP/edit?edit=/log_report.txt

Request to delect file from spiffs
method delect
IP/edit , path=/YLTD.MP3

Handle updload:
update.htm
*/

#include <Arduino.h>
#include <functional>
#include <FS.h>
#include <SD.h>
#include <ESPAsyncWebServer.h>

#define FS_EDITOR_DEBUG    0
#define FS_EDITOR_NAND_TYPE 0
#define FS_EDITOR_SD_TYPE   1

typedef std::function<bool(AsyncWebServerRequest *request)> fs_ArRequestAuthenticateFunction;

class ESPFSEditor: public AsyncWebHandler {
  using File = fs::File;
  using FS = fs::FS;
  public:
    typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;
    typedef std::function<void(AsyncWebServerRequest*)> fs_status;

    ESPFSEditor(const FS& fs, const int fsType, const String& username=String(), const String& password=String());
    /*
      This callback will be called when Update is receiving data
    */
    ESPFSEditor& onProgress(THandlerFunction_Progress fn);
    ESPFSEditor& onStatus(fs_status fn);
    ESPFSEditor& setAuthentication(const char *username, const char *password){  _username = String(username);_password = String(password); return *this; };
    ESPFSEditor& onAuthenticate(fs_ArRequestAuthenticateFunction fn) {_onAuthenticate = fn; return *this;}
    virtual bool canHandle(AsyncWebServerRequest *request) override final;
    virtual void handleRequest(AsyncWebServerRequest *request) override final;
    virtual void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) override final;
    virtual bool isRequestHandlerTrivial() override final {return false;}
    virtual void debug(AsyncWebServerRequest *request);
  private:
    FS _fs;
    String _username;
    String _password; 
    String _uri;
    bool _authenticated;
    uint32_t _startTime;
    uint32_t _filesize;
    uint32_t _progress;
    uint8_t* _edit_htm;
    uint32_t _size_htm;
    fs_status _status_callback;
    THandlerFunction_Progress _progress_callback;
    fs_ArRequestAuthenticateFunction _onAuthenticate;
};

#endif // __ESP_FS_EDITOR_H
