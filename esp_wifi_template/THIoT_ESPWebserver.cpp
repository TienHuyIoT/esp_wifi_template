#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#ifdef ESP32
#include <WiFi.h>
#include <WiFiType.h>
#include <Update.h>
#include "THIoT_ESPEventSignal.h"
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <Updater.h>
#include "flash_hal.h"
#endif
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_SerialTrace.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_SDFSClass.h"
#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_ESPWebserver.h"


#define WEB_SERVER_DBG_PORT SERIAL_PORT
#define WEB_SERVER_DBG_PRINTF(...) SERIAL_LOGI(__VA_ARGS__)
#define WEB_SERVER_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[WEB]", __VA_ARGS__)
#define WEB_TAG_LOG(...) FS_TAG_LOGI("[WEB]", __VA_ARGS__)

static WebserverURLHandleCallbacks defaultCallbacks;

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
    RedirectUrl += ESPConfig.tcpPort();
    RedirectUrl += request->url();

    request->redirect(RedirectUrl);
  }
};

ESPWebserver::ESPWebserver(/* args */)
{
}

ESPWebserver::~ESPWebserver()
{
  this->end();
}

AsyncWebServer* ESPWebserver::_server = nullptr;
AsyncWebServer* ESPWebserver::_server80 = nullptr;
ESPWebsocket* ESPWebserver::_wsHandler = new ESPWebsocket("/ws", "/events");
std::vector<WebserverURLHandleCallbacks*> ESPWebserver::_pUrlCallbacks = {};
asyncHttpHandler ESPWebserver::_httpGetAuthHandler = nullptr;
asyncHttpHandler ESPWebserver::_httpGetHandler = nullptr;
asyncHttpHandler ESPWebserver::_httpPostAuthHandler = nullptr;
ESPFSEditor* ESPWebserver::_spiffsEditor = nullptr;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
ESPFSEditor* ESPWebserver::_sdCardEditor = nullptr;
#endif
String ESPWebserver::_adminAuthUser = String();
String ESPWebserver::_adminAuthPass = String();
String ESPWebserver::_userAuthUser = String();
String ESPWebserver::_userAuthPass = String();
int ESPWebserver::_flashUpdateType = 0;
size_t ESPWebserver::_updateProgress = 0;
uint32_t ESPWebserver::_spiffsUploadPercent = 0;
uint32_t ESPWebserver::_flashUpdatePercent = 0;

void ESPWebserver::onUrlHandle(WebserverURLHandleCallbacks* pCallbacks)
{
  if (pCallbacks != nullptr)
  {
    _pUrlCallbacks.push_back(pCallbacks);
  }
}

void ESPWebserver::fs_editor_status(AsyncWebServerRequest *request)
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

uint8_t ESPWebserver::authentication_level(AsyncWebServerRequest *request)
{
  uint8_t level;
  if (request->authenticate("admin", "20210927"))
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

void ESPWebserver::updatePrintProgress(size_t prg, size_t sz)
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
    snprintf(p, 5, "%u", _flashUpdatePercent);
    _wsHandler->eventsSend(p, "dfu");
  }

  /* Watch dog timer feed */
  wdt_reset();
}

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
uint32_t ESPWebserver::_sdUploadPercent = 0;
void ESPWebserver::sdfsPrintProgress(size_t prg, size_t sz)
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
    snprintf(p, 5, "%u", _sdUploadPercent);
    _wsHandler->eventsSend(p, "sdfs");
  }
  /* Watch dog timer feed */
  wdt_reset();
}
#endif

void ESPWebserver::spiffsPrintProgress(size_t prg, size_t sz)
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
    snprintf(p, 5, "%u", _spiffsUploadPercent);
    _wsHandler->eventsSend(p, "spiffs");
  }
  /* Watch dog timer feed */
  wdt_reset();
}

void ESPWebserver::syncSsidNetworkToEvents()
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
          ESPWifi.ssidScan(json_network);
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

void ESPWebserver::printHandleRequest(AsyncWebServerRequest *request)
{
  String message = "";
  message = "";
  message += "\r\nURI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (HTTP_GET == request->method()) ? "GET" : "POST";
  message += "\nArguments: ";
  message += request->params();
  message += "\n";
  for (uint8_t i = 0; i < request->params(); i++)
  {
    AsyncWebParameter* p = request->getParam(i);
    message += " NAME:" + p->name() + "\n VALUE:" + p->value() + "\n";
  }
  request->send(200, "text/html", message);
  WEB_SERVER_TAG_CONSOLE("[handlerequest] %s", message.c_str());
}

void ESPWebserver::begin(void)
{
  _adminAuthUser = ESPConfig.authAdminUser();
  _adminAuthPass = ESPConfig.authAdminPass();
  _userAuthUser = ESPConfig.authUserUser();
  _userAuthPass = ESPConfig.authUserPass();

  /* Protect server port don't less 80 */
  if (ESPConfig.tcpPort() < 80) {
    // set server port default
    ESPConfig.tcpPortSet(SERVER_PORT_DEFAULT);
  }
  
  WEB_SERVER_TAG_CONSOLE("Init Web Server Port: %u", ESPConfig.tcpPort());
  _server = new AsyncWebServer(ESPConfig.tcpPort());
#ifdef ESP32
  /* Register esp32 event scan done */
  WiFi.onEvent(
      [](WiFiEvent_t event, WiFiEventInfo_t info)
      {
        WEB_SERVER_TAG_CONSOLE("[EVENT] Completed scan for access points");
        String json_network = "{\"status\":\"error\",\"mgs\":\"No network\"}";
        ESPWifi.ssidScan(json_network);
        WEB_SERVER_TAG_CONSOLE("json_network: %s", json_network.c_str());
        _wsHandler->eventsSend(json_network.c_str(), "wifiScan");
      },
      WiFiEvent_t::m_ESP32_EVENT_SCAN_DONE);
#elif defined(ESP8266)
  /* ESP8266 must be register event scan done callback once needing */
#endif
  /* register url callback scan network */
  if (!_pUrlCallbacks.empty())
  {
    for (auto elem : _pUrlCallbacks)
    {
      elem->onScanNetwork(std::bind(&ESPWebserver::syncSsidNetworkToEvents, this));
      using namespace std::placeholders;
      elem->onEventSocket(std::bind(&ESPWebsocket::eventsSend, _wsHandler, _1, _2));
    }
  }

  /* redirect port 80 to tcp port */
  if (ESPConfig.tcpPort() != 80)
  {
    _server80 = new AsyncWebServer(80);
    _server80->addHandler(new RedirectUrlHandler());
    _server80->begin();
  }

  _wsHandler->setHandleCallbacks(new ESPWsDataHandle());
  _wsHandler->begin();
  _server->addHandler(_wsHandler->_ws);
  _server->addHandler(_wsHandler->_events);

  // uri handle = "/edit"
  _spiffsEditor = new ESPFSEditor(NAND_FS_SYSTEM, FS_EDITOR_NAND_TYPE);
  _spiffsEditor->onProgress(spiffsPrintProgress).setAuthentication(_adminAuthUser.c_str(), _adminAuthPass.c_str());
  _spiffsEditor->onStatus(fs_editor_status);
  _server->addHandler(_spiffsEditor);

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  // uri handle = "/edit_sdfs"
  _sdCardEditor = new ESPFSEditor(SD_FS_SYSTEM, FS_EDITOR_SD_TYPE);
  _sdCardEditor->onProgress(sdfsPrintProgress).setAuthentication(_adminAuthUser.c_str(), _adminAuthPass.c_str());
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
              
              if (!_pUrlCallbacks.empty())
              {
                bool hasHandler = false;
                for (auto elem : _pUrlCallbacks)
                {
                  if (elem->onHttpGetAuth(request) != -1)
                  {
                    hasHandler = true;
                    break;
                  }
                }

                if (!hasHandler)
                {
                  printHandleRequest(request);
                }
              }
            }
          });

_server->on("/get_open", HTTP_GET,
          [](AsyncWebServerRequest *request)
          {
            if (_httpGetHandler)
            {
              _httpGetHandler(request);
            }

            if (!_pUrlCallbacks.empty())
            {
              bool hasHandler = false;
              for (auto elem : _pUrlCallbacks)
              {
                if (elem->onHttpGet(request) != -1)
                {
                  break;
                }
              }

              if (!hasHandler)
              {
                printHandleRequest(request);
              }
            }
          });

_server->on("/post", HTTP_POST, [](AsyncWebServerRequest *request)
          {
            if (authentication_level(request) != HTTP_AUTH_FAIL)
            {
              if (_httpPostAuthHandler)
              {
                _httpPostAuthHandler(request);
              }

              if (!_pUrlCallbacks.empty())
              {
                bool hasHandler = false;
                for (auto elem : _pUrlCallbacks)
                {
                  if (elem->onHttpPostAuth(request) != -1)
                  {
                    hasHandler = true;
                    break;
                  }
                }

                if (!hasHandler)
                {
                  printHandleRequest(request);
                }
              }
            }
          });

  /* Serving files in directory. Serving static files with authentication */
  _server->serveStatic("/", NAND_FS_SYSTEM, "/").setDefaultFile("index.htm").setAuthentication(_adminAuthUser.c_str(), _adminAuthPass.c_str());

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  _server->serveStatic("/onsd", SD_FS_SYSTEM, "/").setAuthentication(_adminAuthUser.c_str(), _adminAuthPass.c_str());
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
          WEB_TAG_LOG("[OTA] End");
          if (U_FLASH == _flashUpdateType)
          {
            SOFTReset.enable(500);
          }
          else
          {
            ESPConfig.save();
          }
        }
        else
        {
          updatePrintProgress(101, 100); // 101% --> Fail
          WEB_TAG_LOG("[OTA] Failed");
        }
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
      {
        size_t length = request->contentLength();

        if (!index)
        {
          WEB_SERVER_TAG_CONSOLE("[OTA] file: %s, size=%u", filename.c_str(), length);
          WEB_TAG_LOG("[OTA] file: %s, size = %u", filename.c_str(), length);
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
            // length = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          }
          Update.runAsync(true);
          if (!Update.begin(length, _flashUpdateType))
#else
          _updateProgress = 0;
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
          _updateProgress += len;
          updatePrintProgress(_updateProgress, length);
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
  _server->begin();
}

void ESPWebserver::loop()
{

}

WebserverURLHandleCallbacks::WebserverURLHandleCallbacks() 
: _pScanNetworkCb(nullptr)
{
}
WebserverURLHandleCallbacks::~WebserverURLHandleCallbacks() {}
/**
 * 
 * Handler called after once request with method GET and authenticated.
 */
int WebserverURLHandleCallbacks::onHttpGetAuth(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[WebserverURLHandleCallbacks] >> onHttpGetAuth: default <<");
  return -1;
}
/**
 * Handler called after once request with method GET.
 */
int WebserverURLHandleCallbacks::onHttpGet(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[WebserverURLHandleCallbacks] >> onHttpGet: default <<");
  return -1;
}
/**
 * Handler called after once request with method POST and authenticated.
 */
int WebserverURLHandleCallbacks::onHttpPostAuth(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[WebserverURLHandleCallbacks] >> onHttpPostAuth: default <<");
  return -1;
}
