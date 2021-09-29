#ifndef __ESP_FS_EDITOR_H
#define __ESP_FS_EDITOR_H

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
  public:
    typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;
    typedef std::function<void(AsyncWebServerRequest*)> fs_status;

    ESPFSEditor(const fs::FS& fs, const int fsType, const String& username=String(), const String& password=String());
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
    fs_status _status_callback;
    THandlerFunction_Progress _progress_callback;
    fs_ArRequestAuthenticateFunction _onAuthenticate;
};

#endif // __ESP_FS_EDITOR_H
