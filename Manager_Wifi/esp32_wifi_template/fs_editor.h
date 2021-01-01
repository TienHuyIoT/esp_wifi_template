#ifndef _FS_EDITOR_H_
#define _FS_EDITOR_H_

/*
Request to download file from sd card
IP/edit_sdfs?download=/YLTD.MP3

Request to edit file from sd card
IP/edit_sdfs?edit=/YLTD.MP3

Request to download file from spiffs
IP/edit?download=/log_report.csv

Request to edit file from spiffs
IP/edit?edit=/log_report.txt

Handle updload:
update.htm
*/

#include <FS.h>
#include <ESPAsyncWebServer.h>

#define FS_EDITOR_DEBUG    0

class FSEditor: public AsyncWebHandler {  
  public:
    typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;
#ifdef ESP32
    FSEditor(const fs::FS& fs, const String& uri=String("edit"), const String& username=String(), const String& password=String());
#else
    FSEditor(const String& username=String(), const String& password=String(), const fs::FS& fs=SD, const String& uri=String("edit"));
#endif
    /*
      This callback will be called when Update is receiving data
    */
    FSEditor& onProgress(THandlerFunction_Progress fn);
    FSEditor& setAuthentication(const char *username, const char *password){  _username = String(username);_password = String(password); return *this; };
    virtual bool canHandle(AsyncWebServerRequest *request) override final;
    virtual void handleRequest(AsyncWebServerRequest *request) override final;
    virtual void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) override final;
    virtual bool isRequestHandlerTrivial() override final {return false;}
    virtual void debug(AsyncWebServerRequest *request);
  private:
    fs::FS _fs;
    String _username;
    String _password; 
    String _uri;
    bool _authenticated;
    uint32_t _startTime;
    uint32_t _filesize;
    uint32_t _progress;
    uint8_t* _edit_htm;
    uint32_t _size_htm;
    THandlerFunction_Progress _progress_callback;
};

#endif
