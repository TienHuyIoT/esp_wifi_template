#include <Arduino.h>
#include <FS.h>
#include "THIoT_ESPFsPartHandler.h"
#include "THIoT_ESPSDFSClass.h"
#include "THIoT_PFSerialTrace.h"

#define FS_PART_DBG_PORT SERIAL_PORT
#define FS_PART_DBG_PRINTF(...) SERIAL_TAG_LOGI("[FS_PART]", __VA_ARGS__)

ESPFsPart::ESPFsPart(const fs::FS& fs, const int fsType, const String& username, const String& password)
:_fs(fs)
,_username(username)
,_password(password)
,_authenticated(false)
,_startTime(0)
,_filesize(0)
,_progress(0)
,_progress_callback(NULL)
,_status_callback(NULL)
,logCard("/00000000.csv")
{
  if(FS_PART_SD_TYPE == fsType)
  {
    _uri = "/sd_part";
  }
  else
  {
    _uri = "/fs_part";
  }
}

ESPFsPart& ESPFsPart::onProgress(fs_partHandlerFunction_Progress fn) {
    _progress_callback = fn;
    return *this;
}

ESPFsPart& ESPFsPart::onStatus(fs_partStatus fn)
{
  _status_callback = fn;
  return *this;
}

bool ESPFsPart::canHandle(AsyncWebServerRequest *request){ 
  if(request->url().equalsIgnoreCase(_uri)){
    String path = "";
#if (defined FS_PART_DEBUG) && (FS_PART_DEBUG == 1)
    FS_PART_DBG_PRINTF("canHandle");
    debug(request);
#endif
    if(request->method() == HTTP_GET){
      if(request->hasParam("download")){
        path = request->arg("download");
      }
      else if(request->hasParam("path")){
        path = request->arg("path");
      }
      else
      {
        return false;
      }

#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
      if (!path.startsWith("/"))
      {
        path = "/" + path;
      }
#endif

      request->_tempFile = _fs.open(path, "r");
      if(!request->_tempFile){
        return false;
      }
#ifdef ESP32
      if(request->_tempFile.isDirectory()){
        request->_tempFile.close();
        return false;
      }
#endif

      request->addInterestingHeader("If-Modified-Since");

      logCard.begin(path.c_str());
      params_transaction_card_t tagStart;
      tagStart.date = request->arg("d_start").toInt(); //ddmmyy
      tagStart.time = request->arg("t_start").toInt(); //hhmmss
      if (!logCard.getDownParams(&tagStart))
      {
        request->_tempFile.close();
        return false;
      }
      position_log_t posStart = logCard.posParamsUp();

      params_transaction_card_t tagStop;
      tagStop.date = request->arg("d_stop").toInt(); //ddmmyy
      tagStop.time = request->arg("t_stop").toInt(); //hhmmss
      if (!logCard.getUpParams(&tagStop))
      {
        request->_tempFile.close();
        return false;
      }
      position_log_t posEnd = logCard.posParamsDown();
      logCard.end();

      _filesize = posEnd.end + 2 - posStart.start; // 2: "\r\n"
      FS_PART_DBG_PRINTF("Size: %u", _filesize);

      request->_tempFile.seek(posStart.start);
      _content = request->_tempFile;
      return true;
    }
  }
  return false;
}


void ESPFsPart::handleRequest(AsyncWebServerRequest *request){
  String path = "";
  if(_username.length() && _password.length())
  {
    if(!request->authenticate(_username.c_str(), _password.c_str()))
    {
      return request->requestAuthentication();
    } 
  }     
  else
  {
    if(_onAuthenticate)
    {
      if(!_onAuthenticate(request))
      {
        return;
      }
    }
  }

#if (defined FS_PART_DEBUG) && (FS_PART_DEBUG == 1)
  FS_PART_DBG_PRINTF("handleRequest");
  debug(request);
#endif

  if(request->method() == HTTP_GET){
    if(request->hasParam("path") || request->hasParam("download")){
      String path;
      if (request->hasParam("filename"))
      {
        path = request->arg("filename");
      }
      else
      {
        path = request->_tempFile.name();
      }

      using namespace std::placeholders;  
      AsyncWebServerResponse *response = request->beginResponse("text/html", _filesize,
      std::bind(&ESPFsPart::dataFileHandle, this, _1, _2, _3));
      int filenameStart = path.lastIndexOf('/') + 1;
      char buf[26+path.length()-filenameStart];
      char* filename = (char*)path.c_str() + filenameStart;

      if (request->hasParam("download")) {
        snprintf(buf, sizeof (buf), "attachment; filename=\"%s\"", filename);
      } else {
        snprintf(buf, sizeof (buf), "inline; filename=\"%s\"", filename);
      }
      response->addHeader("Content-Disposition", buf);
      request->send(response);
    }
    else {
      const char * buildTime = __DATE__ " " __TIME__ " GMT";
      if (request->header("If-Modified-Since").equals(buildTime)) {
        request->send(304);
      } else {
        request->send(200, "application/json", "Request fail");
      }
    }
  }
}

size_t ESPFsPart::dataFileHandle(uint8_t* data, size_t len, size_t index)
{
    FS_PART_DBG_PRINTF("dataFileHandle: Len = %u, index = %u", len, index);
    size_t left = _filesize - index;
    if (index >= _filesize)
    {
      _content.close();
      FS_PART_DBG_PRINTF("Close");
      return 0;
    }
    size_t ret;
    if (left > len)
    {
      ret = _content.read(data, len);
    }
    else
    {
      ret = _content.read(data, left);
      _content.close();
      FS_PART_DBG_PRINTF("End");
    }
    FS_PART_DBG_PRINTF("ret = %u", ret);
    return ret;
}

void ESPFsPart::debug(AsyncWebServerRequest *request) {
  if(request->method() == HTTP_GET)
    FS_PART_DBG_PRINTF("GET");
  else if(request->method() == HTTP_POST)
    FS_PART_DBG_PRINTF("POST");
  else if(request->method() == HTTP_DELETE)
    FS_PART_DBG_PRINTF("DELETE");
  else if(request->method() == HTTP_PUT)
    FS_PART_DBG_PRINTF("PUT");
  else if(request->method() == HTTP_PATCH)
    FS_PART_DBG_PRINTF("PATCH");
  else if(request->method() == HTTP_HEAD)
    FS_PART_DBG_PRINTF("HEAD");
  else if(request->method() == HTTP_OPTIONS)
    FS_PART_DBG_PRINTF("OPTIONS");
  else
    FS_PART_DBG_PRINTF("UNKNOWN");
  FS_PART_DBG_PRINTF(" http://%s%s\n", request->host().c_str(), request->url().c_str());

  if(request->contentLength()){
    FS_PART_DBG_PRINTF("_CONTENT_TYPE: %s\n", request->contentType().c_str());
    FS_PART_DBG_PRINTF("_CONTENT_LENGTH: %u\n", request->contentLength());
  }

  int headers = request->headers();
  int i;
  for(i=0;i<headers;i++){
    AsyncWebHeader* h = request->getHeader(i);
    FS_PART_DBG_PRINTF("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
  }

  int params = request->params();
  for(i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isFile()){
      FS_PART_DBG_PRINTF("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
    } else if(p->isPost()){
      FS_PART_DBG_PRINTF("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    } else {
      FS_PART_DBG_PRINTF("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
  }
}
