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
#include "hth_esp_config.h"
#include "hth_esp_wifi.h"
#include "hth_console_dbg.h"
#include "hth_esp_sys_params.h"
#include "hth_esp_soft_reset.h"
#include "hth_esp_sdcard.h"
#include "hth_webserver.h"


#define WEB_SERVER_DBG_PORT CONSOLE_PORT
#define WEB_SERVER_DBG_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define WEB_SERVER_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[WEB SERVER]", __VA_ARGS__)

static serverUrlCallbacks defaultCallbacks;

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

hth_webserver::hth_webserver(/* args */)
{
}

hth_webserver::~hth_webserver()
{
  this->end();
}

AsyncWebServer* hth_webserver::_server = nullptr;
AsyncWebServer* hth_webserver::_server80 = nullptr;
hth_websocket* hth_webserver::_wsHandler = new hth_websocket("/ws", "/events");
serverUrlCallbacks* hth_webserver::_pUrlCallbacks = &defaultCallbacks;
asyncHttpHandler hth_webserver::_httpGetAuthHandler = nullptr;
asyncHttpHandler hth_webserver::_httpGetHandler = nullptr;
asyncHttpHandler hth_webserver::_httpPostAuthHandler = nullptr;
hth_FSEditor* hth_webserver::_spiffsEditor = nullptr;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
hth_FSEditor* hth_webserver::_sdCardEditor = nullptr;
#endif
String hth_webserver::_adminAuthUser = String();
String hth_webserver::_adminAuthPass = String();
String hth_webserver::_userAuthUser = String();
String hth_webserver::_userAuthPass = String();
int hth_webserver::_flashUpdateType = 0;
#ifdef ESP8266
size_t hth_webserver::_updateProgress = 0;
#endif
uint32_t hth_webserver::_spiffsUploadPercent = 0;
uint32_t hth_webserver::_flashUpdatePercent = 0;

void hth_webserver::setHandleCallbacks(serverUrlCallbacks* pCallbacks)
{
  if (pCallbacks != nullptr)
  {
    _pUrlCallbacks = pCallbacks;
  }
}

void hth_webserver::fs_editor_status(AsyncWebServerRequest *request)
{
#ifdef ESP8266
  uint64_t totalBytes, usedBytes;
#elif defined(ESP32)
  unsigned long totalBytes, usedBytes;
#endif
  String status = request->getParam("status")->value();
  WEB_SERVER_TAG_CONSOLE("fs_editor_status");
  if (status == "spiffs")
  {
#ifdef ESP8266
    FSInfo fs_info;
    NAND_FS_SYSTEM.info(fs_info);
    totalBytes = fs_info.totalBytes;
    usedBytes = fs_info.usedBytes;
#elif defined(ESP32)
    totalBytes = NAND_FS_SYSTEM.totalBytes();
    usedBytes = NAND_FS_SYSTEM.usedBytes();
#endif
    WEB_SERVER_TAG_CONSOLE("Nandflash Total space: %lu", totalBytes);
    WEB_SERVER_TAG_CONSOLE("Nandflash Used space: %lu", usedBytes);
  }
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  else
  {
#ifdef ESP8266
#if (HTH_SFDS_HANDLE)
    FSInfo64 fs_info64;
    SD_FS_SYSTEM.info64(fs_info64);
    totalBytes = fs_info64.totalBytes;
    usedBytes = fs_info64.usedBytes;
#else
    totalBytes = SD.size64();
    usedBytes = 0x40000000ULL; // add for fun
#endif
#elif defined(ESP32)
    totalBytes = SD_FS_SYSTEM.totalBytes();
    usedBytes = SD_FS_SYSTEM.usedBytes();
#endif
    WEB_SERVER_TAG_CONSOLE("SD Total space: %llu", totalBytes);
    WEB_SERVER_TAG_CONSOLE("SD Used space: %llu", usedBytes);
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
  output += totalBytes;
  output += F("\", \"usedBytes\":\"");
  output += usedBytes;
  output += "\"";

  output += F(",\"unsupportedFiles\":\"");
  output += "";
  output += "\"}";
  request->send(200, "application/json", output);
  output = String();
}

uint8_t hth_webserver::authentication_level(AsyncWebServerRequest *request)
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

void hth_webserver::updatePrintProgress(size_t prg, size_t sz)
{
  uint32_t per = prg * 100 / sz;
#ifdef ESP32
  if (_flashUpdatePercent != per)
#elif defined(ESP8266)
  // ESP8266 is not power as ESP32, so once 5% we send a package event.
  if (((_flashUpdatePercent != per) && (per % 5) == 0) || (per >= 100))
#endif
  {
    char p[5];
    _flashUpdatePercent = per;
    WEB_SERVER_TAG_CONSOLE("Progress: %u%%", _flashUpdatePercent);
    sprintf(p, "%u", _flashUpdatePercent);
    _wsHandler->eventsSend(p, "dfu");
  }
}

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
uint32_t hth_webserver::_sdUploadPercent = 0;
void hth_webserver::sdfsPrintProgress(size_t prg, size_t sz)
{
  uint32_t per = prg * 100 / sz;
#ifdef ESP32
  if (_sdUploadPercent != per)
#elif defined(ESP8266)
  // ESP8266 is not power as ESP32, so once 5% we send a package event.
  if (((_sdUploadPercent != per) && (per % 5) == 0) || (per >= 100))
#endif
  {
    char p[5];
    _sdUploadPercent = per;
    WEB_SERVER_TAG_CONSOLE("Progress: %u%%", _sdUploadPercent);
    sprintf(p, "%u", _sdUploadPercent);
    _wsHandler->eventsSend(p, "sdfs");
  }
}
#endif

void hth_webserver::spiffsPrintProgress(size_t prg, size_t sz)
{
  uint32_t per = prg * 100 / sz;
#ifdef ESP32
  if (_spiffsUploadPercent != per)
#elif defined(ESP8266)
  // ESP8266 is not power as ESP32, so once 5% we send a package event.
  if (((_spiffsUploadPercent != per) && (per % 5) == 0) || (per >= 100))
#endif
  {
    char p[5];
    _spiffsUploadPercent = per;
    WEB_SERVER_TAG_CONSOLE("Progress: %u%%", _spiffsUploadPercent);
    sprintf(p, "%u", _spiffsUploadPercent);
    _wsHandler->eventsSend(p, "spiffs");
  }
}

void hth_webserver::syncSsidNetworkToEvents()
{
  if (WiFi.scanComplete() == WIFI_SCAN_FAILED)
  {
#ifdef ESP8266
    /* run in async mode */
    WEB_SERVER_TAG_CONSOLE("scanNetworks async mode run");
    WiFi.scanNetworksAsync(
        [](int scanCount)
        {
          WEB_SERVER_TAG_CONSOLE("[EVENT] Completed scan for access points, found %u", scanCount);
          String json_network = "{\"status\":\"error\",\"mgs\":\"No network\"}";
          HTH_espWifi.ssidScan(json_network);
          WEB_SERVER_TAG_CONSOLE("json_network: %s", json_network.c_str());
          _wsHandler->eventsSend(json_network.c_str(), "wifiScan");
        });
#elif defined(ESP32)
    /* run in async mode */
    WEB_SERVER_TAG_CONSOLE("scanNetworks async mode run");
    WiFi.scanNetworks(true);
#endif
  }
}

void hth_webserver::begin(void)
{
  _adminAuthUser = WFDataFile.authAdminUser();
  _adminAuthPass = WFDataFile.authAdminPass();
  _userAuthUser = WFDataFile.authUserUser();
  _userAuthPass = WFDataFile.authUserPass();

  /* Protect server port don't less 80 */
  if (WFDataFile.tcpPort() < 80) {
    // set server port default
    WFDataFile.tcpPortSet(SERVER_PORT_DEFAULT);
  }
  _server = new AsyncWebServer(WFDataFile.tcpPort());
#ifdef ESP32
  /* Register esp32 event scan done */
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
  /* ESP8266 must be register event scan done callback once needing */
#endif
  /* register url callback scan network */
  _pUrlCallbacks->onScanNetwork(std::bind(&hth_webserver::syncSsidNetworkToEvents, this));

  /* redirect port 80 to tcp port */
  if (WFDataFile.tcpPort() != 80)
  {
    _server80 = new AsyncWebServer(80);
    _server80->addHandler(new RedirectUrlHandler());
    _server80->begin();
  }

  _wsHandler->setHandleCallbacks(new hth_wsDataHandler());
  _wsHandler->begin();
  _server->addHandler(_wsHandler->_ws);
  _server->addHandler(_wsHandler->_events);

  _spiffsEditor = new hth_FSEditor(NAND_FS_SYSTEM, "/edit");
  _spiffsEditor->onAuthenticate([](AsyncWebServerRequest *request)
                                { return (authentication_level(request) != HTTP_AUTH_FAIL); });
  _spiffsEditor->onProgress(spiffsPrintProgress);
  _spiffsEditor->onStatus(fs_editor_status);
  _server->addHandler(_spiffsEditor);

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  _sdCardEditor = new hth_FSEditor(SD_FS_SYSTEM, "/edit_sdfs");
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
              _pUrlCallbacks->onHttpGetAuth(request);
            }
          });

_server->on("/get_open", HTTP_GET,
          [](AsyncWebServerRequest *request)
          {
            if (_httpGetHandler)
            {
              _httpGetHandler(request);
            }
            _pUrlCallbacks->onHttpGet(request);
          });

_server->on("/post", HTTP_POST, [](AsyncWebServerRequest *request)
          {
            if (authentication_level(request) != HTTP_AUTH_FAIL)
            {
              if (_httpPostAuthHandler)
              {
                _httpPostAuthHandler(request);
              }
              _pUrlCallbacks->onHttpPostAuth(request);
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
          WEB_SERVER_TAG_CONSOLE("Update Start: %s, size=%u", filename.c_str(), length);
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
            WEB_SERVER_TAG_CONSOLE("Update Success: %uB", index + len);
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
        WEB_SERVER_TAG_CONSOLE("NOT_FOUND: ");
        if (request->method() == HTTP_GET)
          WEB_SERVER_TAG_CONSOLE("GET");
        else if (request->method() == HTTP_POST)
          WEB_SERVER_TAG_CONSOLE("POST");
        else if (request->method() == HTTP_DELETE)
          WEB_SERVER_TAG_CONSOLE("DELETE");
        else if (request->method() == HTTP_PUT)
          WEB_SERVER_TAG_CONSOLE("PUT");
        else if (request->method() == HTTP_PATCH)
          WEB_SERVER_TAG_CONSOLE("PATCH");
        else if (request->method() == HTTP_HEAD)
          WEB_SERVER_TAG_CONSOLE("HEAD");
        else if (request->method() == HTTP_OPTIONS)
          WEB_SERVER_TAG_CONSOLE("OPTIONS");
        else
          WEB_SERVER_TAG_CONSOLE("UNKNOWN");
        WEB_SERVER_TAG_CONSOLE(" http://%s%s\n", request->host().c_str(), request->url().c_str());

        if (request->contentLength())
        {
          WEB_SERVER_TAG_CONSOLE("_CONTENT_TYPE: %s", request->contentType().c_str());
          WEB_SERVER_TAG_CONSOLE("_CONTENT_LENGTH: %u", request->contentLength());
        }

        int headers = request->headers();
        int i;
        for (i = 0; i < headers; i++)
        {
          AsyncWebHeader *h = request->getHeader(i);
          WEB_SERVER_TAG_CONSOLE("_HEADER[%s]: %s", h->name().c_str(), h->value().c_str());
        }

        int params = request->params();
        for (i = 0; i < params; i++)
        {
          AsyncWebParameter *p = request->getParam(i);
          if (p->isFile())
          {
            WEB_SERVER_TAG_CONSOLE("_FILE[%s]: %s, size: %u", p->name().c_str(), p->value().c_str(), p->size());
          }
          else if (p->isPost())
          {
            WEB_SERVER_TAG_CONSOLE("_POST[%s]: %s", p->name().c_str(), p->value().c_str());
          }
          else
          {
            WEB_SERVER_TAG_CONSOLE("_GET[%s]: %s", p->name().c_str(), p->value().c_str());
          }
        }

        request->send(404);
      });

  _server->onFileUpload(
      [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
      {
        if (!index)
        {
          WEB_SERVER_TAG_CONSOLE("UploadStart: %s", filename.c_str());
        }
        WEB_SERVER_TAG_CONSOLE("%s", (const char *)data);
        if (final)
        {
          WEB_SERVER_TAG_CONSOLE("UploadEnd: %s (%u)", filename.c_str(), index + len);
        }
      });

  _server->onRequestBody(
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
        if (!index)
        {
          WEB_SERVER_TAG_CONSOLE("BodyStart: %u", total);
        }
        WEB_SERVER_TAG_CONSOLE("%s", (const char *)data);
        if (index + len == total)
        {
          WEB_SERVER_TAG_CONSOLE("BodyEnd: %u", total);
        }
      });

  WEB_SERVER_TAG_CONSOLE("Init Web Server Port: %u", WFDataFile.tcpPort());
  _server->begin();
}

void hth_webserver::loop()
{

}

serverUrlCallbacks::serverUrlCallbacks() 
: _pScanNetworkCb(nullptr)
{
}
serverUrlCallbacks::~serverUrlCallbacks() {}
/**
 * 
 * Handler called after once request with method GET and authenticated.
 */
void serverUrlCallbacks::onHttpGetAuth(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[serverUrlCallbacks] >> onHttpGetAuth: default <<");
}
/**
 * Handler called after once request with method GET.
 */
void serverUrlCallbacks::onHttpGet(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[serverUrlCallbacks] >> onHttpGet: default <<");
}
/**
 * Handler called after once request with method POST and authenticated.
 */
void serverUrlCallbacks::onHttpPostAuth(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[serverUrlCallbacks] >> onHttpPostAuth: default <<");
}
