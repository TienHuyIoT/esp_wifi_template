#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#ifdef ESP32
#include <Update.h>
#elif defined(ESP8266)
#include <Updater.h>
#include "flash_hal.h"
#endif
#include "app_config.h"
#include "hth_console_dbg.h"
#include "hth_esp_sys_data.h"
#include "hth_esp_soft_reset.h"
#include "async_webserver.h"

#define WEB_SERVER_DBG_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define WEB_SERVER_DBG_PORT CONSOLE_PORT

class RedirectUrlHandler : public AsyncWebHandler
{
public:
  RedirectUrlHandler() {}
  virtual ~RedirectUrlHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    String RedirectUrl = "http://";

    RedirectUrl += request->host();
    RedirectUrl += ":";
    RedirectUrl += WFDataFile.tcpPort();
    RedirectUrl += request->url();

    request->redirect(RedirectUrl);
  }
};

async_webserver::async_webserver(/* args */)
{
}

async_webserver::~async_webserver()
{
  this->end();
}

AsyncWebServer* async_webserver::_server = new AsyncWebServer(25123);
AsyncWebServer* async_webserver::_server80 = new AsyncWebServer(80);
asyncHttpHandler async_webserver::_httpGetAuthHandler = nullptr;
asyncHttpHandler async_webserver::_httpGetHandler = nullptr;
asyncHttpHandler async_webserver::_httpPostAuthHandler = nullptr;
FSEditor* async_webserver::_spiffsEditor = new FSEditor(NAND_FS_SYSTEM, "/edit");
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
FSEditor* async_webserver::_sdCardEditor = new FSEditor(SD_FS_SYSTEM, "/edit_sdfs");
#endif
String async_webserver::_adminAuthUser = String();
String async_webserver::_adminAuthPass = String();
String async_webserver::_userAuthUser = String();
String async_webserver::_userAuthPass = String();
int async_webserver::_flashUpdateType = 0;
uint32_t async_webserver::_spiffsUploadPercent = 0;
uint32_t async_webserver::_flashUpdatePercent = 0;

void async_webserver::fs_editor_status(AsyncWebServerRequest *request)
{
  char buf_ttb[64];
  char buf_udb[64];
  String status = request->getParam("status")->value();

  if (status == "spiffs")
  {
#ifdef ESP8266
    FSInfo fs_info;
    NAND_FS_SYSTEM.info(fs_info);
    sprintf(buf_ttb, "%lu", fs_info.totalBytes);
    sprintf(buf_udb, "%lu", fs_info.usedBytes);
    WEB_SERVER_DBG_PORT.printf("\nNandflash Total space: %lu\r\n", fs_info.totalBytes);
    WEB_SERVER_DBG_PORT.printf("Nandflash Used space: %lu\r\n", fs_info.usedBytes);
#elif defined(ESP32)
    sprintf(buf_ttb, "%lu", NAND_FS_SYSTEM.totalBytes());
    sprintf(buf_udb, "%lu", NAND_FS_SYSTEM.usedBytes());
    WEB_SERVER_DBG_PORT.printf("\nNandflash Total space: %lu\r\n", NAND_FS_SYSTEM.totalBytes());
    WEB_SERVER_DBG_PORT.printf("Nandflash Used space: %lu\r\n", NAND_FS_SYSTEM.usedBytes());
#endif
  }
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  else
  {
    sprintf(buf_ttb, "%llu", SD_FS_SYSTEM.totalBytes());
    sprintf(buf_udb, "%llu", SD_FS_SYSTEM.usedBytes());
    WEB_SERVER_DBG_PORT.printf("\nSD Total space: %lu\r\n", SD_FS_SYSTEM.totalBytes());
    WEB_SERVER_DBG_PORT.printf("SD Used space: %lu\r\n", SD_FS_SYSTEM.usedBytes());
  }
#endif

  String output = "{";

  output += "\"type\":\"";
  if (status == "spiffs")
  {
    output += "ESP";
  }
  else
  {
    output += "SD";
  }
  output += "\", \"isOk\":";

  output += F("\"true\", \"totalBytes\":\"");
  output += buf_ttb;
  output += F("\", \"usedBytes\":\"");
  output += buf_udb;
  output += "\"";

  output += F(",\"unsupportedFiles\":\"");
  output += "";
  output += "\"}";
  request->send(200, "application/json", output);
  output = String();
}

uint8_t async_webserver::authentication_level(AsyncWebServerRequest *request)
{
  uint8_t level;
  if (request->authenticate("admin", "25123"))
  {
    level = HTTP_AUTH_LV0;
  }
  else if(request->authenticate(_adminAuthUser.c_str(), _adminAuthPass.c_str()))
  {
    level = HTTP_AUTH_LV0;
  }
  else if(request->authenticate(_userAuthUser.c_str(), _userAuthPass.c_str()))
  {
    level = HTTP_AUTH_LV1;
  }
  else
  {
    level = HTTP_AUTH_FAIL;
    request->requestAuthentication();
  }

  return level;
}

void async_webserver::update_printProgress(size_t prg, size_t sz)
{
  uint32_t per = prg * 100 / sz;
  if (_flashUpdatePercent != per)
  {
    char p[5];
    _flashUpdatePercent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\r\n", _flashUpdatePercent);
    sprintf(p, "%u", _flashUpdatePercent);
    // events.send(p, "dfu");
  }
}

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
uint32_t async_webserver::_sdUploadPercent = 0;
void async_webserver::sdfs_printProgress(size_t prg, size_t sz)
{
  uint32_t per = prg * 100 / sz;
  if (_sdUploadPercent != per)
  {
    char p[5];
    _sdUploadPercent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\r\n", _sdUploadPercent);
    sprintf(p, "%u", _sdUploadPercent);
    // events.send(p, "sdfs");
  }
}
#endif

void async_webserver::spiffs_printProgress(size_t prg, size_t sz)
{
  uint32_t per = prg * 100 / sz;
  if (_spiffsUploadPercent != per)
  {
    char p[5];
    _spiffsUploadPercent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\r\n", _spiffsUploadPercent);
    sprintf(p, "%u", _spiffsUploadPercent);
    // events.send(p, "spiffs");
  }
}

void async_webserver::begin(void)
{
  _adminAuthUser = WFDataFile.authAdminUser();
  _adminAuthPass = WFDataFile.authAdminPass();
  _userAuthUser = WFDataFile.authUserUser();
  _userAuthPass = WFDataFile.authUserPass();

  /* redirect port 80 to tcp port */
  if (WFDataFile.tcpPort() != 80)
  {
    _server80->addHandler(new RedirectUrlHandler());
  }

  // ws.onEvent(onWsEvent);
  // _server->addHandler(&ws);

  // events.onConnect([](AsyncEventSourceClient *client){
  //   client->send("hello!",NULL,millis(),1000);
  //   WEB_SERVER_DBG_PRINTF("\r\nevents connect: %u", client->lastId());
  // });
  // _server->addHandler(&events);

  _spiffsEditor->onAuthenticate([](AsyncWebServerRequest *request)
                                { return (authentication_level(request) != HTTP_AUTH_FAIL); });
  _spiffsEditor->onProgress(spiffs_printProgress);
  _spiffsEditor->onStatus(fs_editor_status);
  _server->addHandler(_spiffsEditor);

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  _sdCardEditor->onAuthenticate([](AsyncWebServerRequest *request)
                            { return (authentication_level(request) != HTTP_AUTH_FAIL); });
  _sdCardEditor->onProgress(sdfs_printProgress);
  _sdCardEditor->onStatus(fs_editor_status);
  _server->addHandler(_sdCardEditor);
#endif

  if (_httpGetAuthHandler)
  {
    _server->on(_uriHttpGetAuth.c_str(), HTTP_GET, [](AsyncWebServerRequest *request)
              {
                if (authentication_level(request) != HTTP_AUTH_FAIL)
                {
                  _httpGetAuthHandler(request);
                }
              });
  }

  if (_httpGetHandler)
  {
    _server->on(_uriHttpGet.c_str(), HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                _httpGetHandler(request);
              });
  }

  if (_httpPostAuthHandler)
  {
    _server->on(_uriHttpPostAuth.c_str(), HTTP_POST, [](AsyncWebServerRequest *request)
              {
                if (authentication_level(request) != HTTP_AUTH_FAIL)
                {
                  _httpPostAuthHandler(request);
                }
              });
  }

  /* Serving files in directory. Serving static files with authentication */
  // _server->serveStatic("/", NAND_FS_SYSTEM, "/").setAuthentication(_adminAuthUser, _adminAuthPass);
  _server->serveStatic("/", NAND_FS_SYSTEM, "/").setDefaultFile("index.htm").onAuthenticate([](AsyncWebServerRequest *request)
                                                                                           { return (authentication_level(request) != HTTP_AUTH_FAIL); });

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  _server->serveStatic("/onsd", SD_FS_SYSTEM, "/").onAuthenticate([](AsyncWebServerRequest *request)
                                                                 { return (authentication_level(request) != HTTP_AUTH_FAIL); });
#endif

  Update.onProgress(update_printProgress);
  // Simple Firmware Update Form
  _server->on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
             {
               if (authentication_level(request) == HTTP_AUTH_LV0)
               {
                 request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
               }
             });
  _server->on(
      "/update", HTTP_POST, [](AsyncWebServerRequest *request)
      {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", !Update.hasError() ? "Update OK" : "Update FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
        if (!Update.hasError())
        {
          update_printProgress(100, 100); // 100%
          if (U_FLASH == _flashUpdateType)
          {
            HTH_softReset.enable(500);
          }
          else
          {
            WFDataFile.commitToFS();
          }
        }
        else
        {
          update_printProgress(101, 100); // 101%
        }
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
      {
        if (!index)
        {
          size_t length = request->contentLength();
          WEB_SERVER_DBG_PORT.printf("Update Start: %s, size=%u\r\n", filename.c_str(), length);
#ifdef ESP8266
          // if filename includes spiffs, update the spiffs partition
          if ((filename.indexOf("spiffs") > -1) || (filename.indexOf("littlefs") > -1))
          {
            _flashUpdateType = U_FS;
            length = ((size_t) &_FS_end - (size_t) &_FS_start);
          }
          else
          {
            _flashUpdateType = U_FLASH;
            length = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          }
          Update.runAsync(true);
          if (!Update.begin(length, _flashUpdateType))
#else
          // if filename includes spiffs, update the spiffs partition
          if ((filename.indexOf("spiffs") > -1) || (filename.indexOf("littlefs") > -1))
          {
            _flashUpdateType = U_SPIFFS;
            length = UPDATE_SIZE_UNKNOWN;
          }
          else
          {
            _flashUpdateType = U_FLASH;
          }

          if (!Update.begin(length, _flashUpdateType))
#endif
          {
            Update.printError(WEB_SERVER_DBG_PORT);
          }
        }
        if (!Update.hasError())
        {
          if (Update.write(data, len) != len)
          {
            Update.printError(WEB_SERVER_DBG_PORT);
          }
        }
        if (final)
        {
          if (Update.end(true))
          {
            WEB_SERVER_DBG_PORT.printf("Update Success: %uB\r\n", index + len);
          }
          else
          {
            Update.printError(WEB_SERVER_DBG_PORT);
          }
        }
      });

  _server->onNotFound(
      [](AsyncWebServerRequest *request)
      {
        WEB_SERVER_DBG_PORT.printf("NOT_FOUND: ");
        if (request->method() == HTTP_GET)
          WEB_SERVER_DBG_PORT.printf("GET");
        else if (request->method() == HTTP_POST)
          WEB_SERVER_DBG_PORT.printf("POST");
        else if (request->method() == HTTP_DELETE)
          WEB_SERVER_DBG_PORT.printf("DELETE");
        else if (request->method() == HTTP_PUT)
          WEB_SERVER_DBG_PORT.printf("PUT");
        else if (request->method() == HTTP_PATCH)
          WEB_SERVER_DBG_PORT.printf("PATCH");
        else if (request->method() == HTTP_HEAD)
          WEB_SERVER_DBG_PORT.printf("HEAD");
        else if (request->method() == HTTP_OPTIONS)
          WEB_SERVER_DBG_PORT.printf("OPTIONS");
        else
          WEB_SERVER_DBG_PORT.printf("UNKNOWN");
        WEB_SERVER_DBG_PORT.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

        if (request->contentLength())
        {
          WEB_SERVER_DBG_PORT.printf("_CONTENT_TYPE: %s\r\n", request->contentType().c_str());
          WEB_SERVER_DBG_PORT.printf("_CONTENT_LENGTH: %u\r\n", request->contentLength());
        }

        int headers = request->headers();
        int i;
        for (i = 0; i < headers; i++)
        {
          AsyncWebHeader *h = request->getHeader(i);
          WEB_SERVER_DBG_PORT.printf("_HEADER[%s]: %s\r\n", h->name().c_str(), h->value().c_str());
        }

        int params = request->params();
        for (i = 0; i < params; i++)
        {
          AsyncWebParameter *p = request->getParam(i);
          if (p->isFile())
          {
            WEB_SERVER_DBG_PORT.printf("_FILE[%s]: %s, size: %u\r\n", p->name().c_str(), p->value().c_str(), p->size());
          }
          else if (p->isPost())
          {
            WEB_SERVER_DBG_PORT.printf("_POST[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
          }
          else
          {
            WEB_SERVER_DBG_PORT.printf("_GET[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
          }
        }

        request->send(404);
      });

  _server->onFileUpload(
      [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
      {
        if (!index)
        {
          WEB_SERVER_DBG_PORT.printf("UploadStart: %s\r\n", filename.c_str());
        }
        WEB_SERVER_DBG_PORT.printf("%s", (const char *)data);
        if (final)
        {
          WEB_SERVER_DBG_PORT.printf("UploadEnd: %s (%u)\r\n", filename.c_str(), index + len);
        }
      });

  _server->onRequestBody(
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
        if (!index)
        {
          WEB_SERVER_DBG_PORT.printf("BodyStart: %u\r\n", total);
        }
        WEB_SERVER_DBG_PORT.printf("%s", (const char *)data);
        if (index + len == total)
        {
          WEB_SERVER_DBG_PORT.printf("BodyEnd: %u\r\n", total);
        }
      });

  if (WFDataFile.tcpPort() < 80)
  {
    WFDataFile.tcpPortSet(25123);
  }

  WEB_SERVER_DBG_PRINTF("\r\nInit Web Server Port: %u\r\n", WFDataFile.tcpPort());

  _server->begin(WFDataFile.tcpPort());
  if (WFDataFile.tcpPort() != 80)
  {
    _server80->begin();
  }
}

void async_webserver::loop()
{

}
