#ifndef _SD_EDITOR_H_
#define _SD_EDITOR_H_

/*
Request to download file from sd card
http://192.168.1.9/edit_sdfs?download=/YLTD.MP3

Request to download file from spiffs
http://192.168.1.9/edit?download=/log_report.csv

Handle updload:
update.htm
*/

#include <FS.h>
#include <ESPAsyncWebServer.h>

#define SD_EDITOR_DEBUG    0

class SDEditor: public AsyncWebHandler {
  private:
    fs::FS _fs;
    String _username;
    String _password; 
    bool _authenticated;
    uint32_t _startTime;
    uint32_t _filesize;
  public:
#ifdef ESP32
    SDEditor(const fs::FS& fs, const String& username=String(), const String& password=String());
#else
    SDEditor(const String& username=String(), const String& password=String(), const fs::FS& fs=SD);
#endif
    virtual bool canHandle(AsyncWebServerRequest *request) override final;
    virtual void handleRequest(AsyncWebServerRequest *request) override final;
    virtual void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) override final;
    virtual bool isRequestHandlerTrivial() override final {return false;}
    virtual void debug(AsyncWebServerRequest *request);
};

#endif
