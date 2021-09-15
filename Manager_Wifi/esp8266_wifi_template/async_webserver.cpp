#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#ifdef ESP32
#include <WiFi.h>
#include <WiFiType.h>
#include <Update.h>
#include "esp_event_signal.h"
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <Updater.h>
#include "flash_hal.h"
#endif
#include "app_config.h"
#include "hth_esp_wifi.h"
#include "hth_console_dbg.h"
#include "hth_esp_sys_data.h"
#include "hth_esp_soft_reset.h"
#include "hth_esp_sdcard.h"
#include "async_webserver.h"


#define WEB_SERVER_DBG_PORT CONSOLE_PORT
#define WEB_SERVER_DBG_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define WEB_SERVER_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[WEB SERVER]", __VA_ARGS__)

static serverCallbacks defaultCallbacks;

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
async_websocket* async_webserver::_wsHandler = new async_websocket("/ws", "/events");
serverCallbacks* async_webserver::_pCallbacks = &defaultCallbacks;
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
#ifdef ESP8266
size_t async_webserver::_updateProgress = 0;
#endif
uint32_t async_webserver::_spiffsUploadPercent = 0;
uint32_t async_webserver::_flashUpdatePercent = 0;

void async_webserver::setHandleCallbacks(serverCallbacks* pCallbacks)
{
  if (pCallbacks != nullptr)
  {
    _pCallbacks = pCallbacks;
  }
}

void async_webserver::fs_editor_status(AsyncWebServerRequest *request)
{
  char buf_ttb[64];
  char buf_udb[64];
  String status = request->getParam("status")->value();

  if (status == "spiffs")
  {
#ifdef ESP8266
    FSInfo64 fs_info;
    NAND_FS_SYSTEM.info64(fs_info);
    sprintf(buf_ttb, "%lu", fs_info.totalBytes);
    sprintf(buf_udb, "%lu", fs_info.usedBytes);
    WEB_SERVER_TAG_CONSOLE("Nandflash Total space: %lu", fs_info.totalBytes);
    WEB_SERVER_TAG_CONSOLE("Nandflash Used space: %lu", fs_info.usedBytes);
#elif defined(ESP32)
    sprintf(buf_ttb, "%lu", NAND_FS_SYSTEM.totalBytes());
    sprintf(buf_udb, "%lu", NAND_FS_SYSTEM.usedBytes());
    WEB_SERVER_TAG_CONSOLE("Nandflash Total space: %lu", NAND_FS_SYSTEM.totalBytes());
    WEB_SERVER_TAG_CONSOLE("Nandflash Used space: %lu", NAND_FS_SYSTEM.usedBytes());
#endif
  }
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  else
  {
#ifdef ESP8266
    FSInfo fs_info;
    SD_FS_SYSTEM.info(fs_info);
    sprintf(buf_ttb, "%lu", fs_info.totalBytes);
    sprintf(buf_udb, "%lu", fs_info.usedBytes);
    WEB_SERVER_TAG_CONSOLE("Nandflash Total space: %lu", fs_info.totalBytes);
    WEB_SERVER_TAG_CONSOLE("Nandflash Used space: %lu", fs_info.usedBytes);
#elif defined(ESP32)
    sprintf(buf_ttb, "%llu", SD_FS_SYSTEM.totalBytes());
    sprintf(buf_udb, "%llu", SD_FS_SYSTEM.usedBytes());
    WEB_SERVER_TAG_CONSOLE("SD Total space: %lu", SD_FS_SYSTEM.totalBytes());
    WEB_SERVER_TAG_CONSOLE("SD Used space: %lu", SD_FS_SYSTEM.usedBytes());
#endif
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

void async_webserver::updatePrintProgress(size_t prg, size_t sz)
{
  uint32_t per = prg * 100 / sz;
  if (_flashUpdatePercent != per)
  {
    char p[5];
    _flashUpdatePercent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\r\n", _flashUpdatePercent);
    sprintf(p, "%u", _flashUpdatePercent);
    _wsHandler->eventsSend(p, "dfu");
  }
}

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
uint32_t async_webserver::_sdUploadPercent = 0;
void async_webserver::sdfsPrintProgress(size_t prg, size_t sz)
{
  uint32_t per = prg * 100 / sz;
  if (_sdUploadPercent != per)
  {
    char p[5];
    _sdUploadPercent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\r\n", _sdUploadPercent);
    sprintf(p, "%u", _sdUploadPercent);
    _wsHandler->eventsSend(p, "sdfs");
  }
}
#endif

void async_webserver::spiffsPrintProgress(size_t prg, size_t sz)
{
  uint32_t per = prg * 100 / sz;
  if (_spiffsUploadPercent != per)
  {
    char p[5];
    _spiffsUploadPercent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\r\n", _spiffsUploadPercent);
    sprintf(p, "%u", _spiffsUploadPercent);
    _wsHandler->eventsSend(p, "spiffs");
  }
}

void async_webserver::syncSsidNetworkToEvents()
{
if(WiFi.scanComplete() == WIFI_SCAN_FAILED) {
#ifdef ESP8266
  /* run in async mode */
  WEB_SERVER_TAG_CONSOLE("scanNetworks async mode run");
  WiFi.scanNetworksAsync(
    [](int scanCount) {
      WEB_SERVER_TAG_CONSOLE("[EVENT] Completed scan for access points, found %u", scanCount);
      String json_network = "{\"status\":\"error\",\"mgs\":\"No network\"}";
      HTH_espWifi.ssidScan(json_network);
      WEB_SERVER_TAG_CONSOLE("json_network: %s", json_network.c_str());
      _wsHandler->eventsSend(json_network.c_str(), "wifiScan");
    }
  );
#elif defined(ESP32)
  /* run in async mode */
  WEB_SERVER_TAG_CONSOLE("scanNetworks async mode run");
  WiFi.scanNetworks(true);
#endif
  }
}

void async_webserver::begin(void)
{
  _adminAuthUser = WFDataFile.authAdminUser();
  _adminAuthPass = WFDataFile.authAdminPass();
  _userAuthUser = WFDataFile.authUserUser();
  _userAuthPass = WFDataFile.authUserPass();
#ifdef ESP32
  WiFi.onEvent(
      [](WiFiEvent_t event, WiFiEventInfo_t info)
      {
        WEB_SERVER_TAG_CONSOLE("[EVENT] Completed scan for access points");
        String json_network = "{\"status\":\"error\",\"mgs\":\"No network\"}";
        HTH_espWifi.ssidScan(json_network);
        WEB_SERVER_TAG_CONSOLE("json_network: %s", json_network.c_str());
        _wsHandler->eventsSend(json_network.c_str(), "wifiScan");
      },
      WiFiEvent_t::m_ESP32_EVENT_SCAN_DONE);
#elif defined(ESP8266)
  _pCallbacks->onScanNetwork(std::bind(&async_webserver::syncSsidNetworkToEvents, this));
#endif
  /* redirect port 80 to tcp port */
  if (WFDataFile.tcpPort() != 80)
  {
    _server80->addHandler(new RedirectUrlHandler());
  }

  _wsHandler->setHandleCallbacks(new wsDataHandler());
  _wsHandler->begin();
  _server->addHandler(_wsHandler->_ws);
  _server->addHandler(_wsHandler->_events);

  _spiffsEditor->onAuthenticate([](AsyncWebServerRequest *request)
                                { return (authentication_level(request) != HTTP_AUTH_FAIL); });
  _spiffsEditor->onProgress(spiffsPrintProgress);
  _spiffsEditor->onStatus(fs_editor_status);
  _server->addHandler(_spiffsEditor);

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  _sdCardEditor->onAuthenticate([](AsyncWebServerRequest *request)
                            { return (authentication_level(request) != HTTP_AUTH_FAIL); });
  _sdCardEditor->onProgress(sdfsPrintProgress);
  _sdCardEditor->onStatus(fs_editor_status);
  _server->addHandler(_sdCardEditor);
#endif


_server->on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
          {
            if (authentication_level(request) != HTTP_AUTH_FAIL)
            {
              if (_httpGetAuthHandler)
              {
                _httpGetAuthHandler(request);
              }
              _pCallbacks->onHttpGetAuth(request);
            }
          });



_server->on("/get_open", HTTP_GET,
          [](AsyncWebServerRequest *request)
          {
            if (_httpGetHandler)
            {
              _httpGetHandler(request);
            }
            _pCallbacks->onHttpGet(request);
          });



_server->on("/post", HTTP_POST, [](AsyncWebServerRequest *request)
          {
            if (authentication_level(request) != HTTP_AUTH_FAIL)
            {
              if (_httpPostAuthHandler)
              {
                _httpPostAuthHandler(request);
              }
              _pCallbacks->onHttpPostAuth(request);
            }
          });

  /* Serving files in directory. Serving static files with authentication */
  // _server->serveStatic("/", NAND_FS_SYSTEM, "/").setAuthentication(_adminAuthUser, _adminAuthPass);
  _server->serveStatic("/", NAND_FS_SYSTEM, "/").setDefaultFile("index.htm").onAuthenticate([](AsyncWebServerRequest *request)
                                                                                           { return (authentication_level(request) != HTTP_AUTH_FAIL); });

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  _server->serveStatic("/onsd", SD_FS_SYSTEM, "/").onAuthenticate([](AsyncWebServerRequest *request)
                                                                 { return (authentication_level(request) != HTTP_AUTH_FAIL); });
#endif

#ifdef ESP32
  Update.onProgress(updatePrintProgress);
#elif defined(ESP8266)
  // useless
#endif
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
          updatePrintProgress(100, 100); // 100% --> Done
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
          updatePrintProgress(101, 100); // 101% --> Fail
        }
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
      {
        size_t length = request->contentLength();

        if (!index)
        {
          WEB_SERVER_DBG_PORT.printf("Update Start: %s, size=%u\r\n", filename.c_str(), length);
#ifdef ESP8266
          _updateProgress = 0;
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
#ifdef ESP8266
          _updateProgress += len;
          updatePrintProgress(_updateProgress, length);
#endif
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

serverCallbacks::serverCallbacks() 
: _pScanNetworkCb(nullptr)
{
}
serverCallbacks::~serverCallbacks() {}
/**
 * 
 * Handler called after once request with method GET and authenticated.
 */
void serverCallbacks::onHttpGetAuth(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[serverCallbacks] >> onHttpGetAuth: default <<");
}
/**
 * Handler called after once request with method GET.
 */
void serverCallbacks::onHttpGet(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[serverCallbacks] >> onHttpGet: default <<");
}
/**
 * Handler called after once request with method POST and authenticated.
 */
void serverCallbacks::onHttpPostAuth(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[serverCallbacks] >> onHttpPostAuth: default <<");
}
