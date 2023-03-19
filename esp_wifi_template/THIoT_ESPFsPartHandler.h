/**
 * /sd_part?download=/00000000.csv&filename=20230319_00000000.csv&d_start=ddmmyy&t_start=hhmmss&d_stop=ddmmyy&t_stop=hhmmss
 * /fs_part?download=/5FCF32E7.csv&filename=20230319_5FCF32E7.csv&d_start=160123&t_start=000000&d_stop=170323&t_stop=000000
*/

#ifndef __ESP_FS_PART_H
#define __ESP_FS_PART_H

#include <Arduino.h>
#include <functional>
#include <FS.h>
#include <SD.h>
#include <ESPAsyncWebServer.h>
#include "THIoT_PFCsvFileHandler.h"

#define FS_PART_DEBUG     1
#define FS_PART_NAND_TYPE 0
#define FS_PART_SD_TYPE   1

typedef std::function<bool(AsyncWebServerRequest *request)> fs_partRequestAuthenticateFunction;

class ESPFsPart: public AsyncWebHandler {  
  public:
    typedef std::function<void(size_t, size_t)> fs_partHandlerFunction_Progress;
    typedef std::function<void(AsyncWebServerRequest*)> fs_partStatus;

    ESPFsPart(const fs::FS& fs, const int fsType, const String& username=String(), const String& password=String());
    /*
      This callback will be called when Update is receiving data
    */
    ESPFsPart& onProgress(fs_partHandlerFunction_Progress fn);
    ESPFsPart& onStatus(fs_partStatus fn);
    ESPFsPart& setAuthentication(const char *username, const char *password){  _username = String(username);_password = String(password); return *this; };
    ESPFsPart& onAuthenticate(fs_partRequestAuthenticateFunction fn) {_onAuthenticate = fn; return *this;}
    virtual bool canHandle(AsyncWebServerRequest *request) override final;
    virtual void handleRequest(AsyncWebServerRequest *request) override final;
    virtual bool isRequestHandlerTrivial() override final {return false;}
    virtual void debug(AsyncWebServerRequest *request);
    size_t dataFileHandle(uint8_t* data, size_t len, size_t index);
  private:
    fs::FS _fs;
    File _content;
    String _username;
    String _password; 
    String _uri;
    bool _authenticated;
    uint32_t _startTime;
    uint32_t _filesize;
    uint32_t _progress;
    uint8_t* _edit_htm;
    uint32_t _size_htm;
    fs_partStatus _status_callback;
    fs_partHandlerFunction_Progress _progress_callback;
    fs_partRequestAuthenticateFunction _onAuthenticate;
    LOGTransactionCardClass logCard;
};

#endif // __ESP_FS_PART_H
