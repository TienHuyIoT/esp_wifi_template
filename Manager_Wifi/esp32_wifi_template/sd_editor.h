#ifndef _SD_EDITOR_H_
#define _SD_EDITOR_H_

#include <FS.h>
#include <ESPAsyncWebServer.h>

class SDEditor: public AsyncWebHandler {
  private:
    fs::FS _fs;
    String _username;
    String _password; 
    bool _authenticated;
    uint32_t _startTime;
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
};

#endif