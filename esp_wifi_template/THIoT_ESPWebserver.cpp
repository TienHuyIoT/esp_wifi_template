#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#ifdef ESP32
#include <WiFi.h>
#include <WiFiType.h>
#include <Update.h>
#include "esp_task_wdt.h"
#include "THIoT_ESPEventSignal.h"
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <Updater.h>
#include "flash_hal.h"
#endif
#include "THIoT_ESPConfig.h"
#include "THIoT_APPConfig.h"
#include "THIoT_ESPEEPromParams.h"
#include "THIoT_PFTicker.h"
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPSDFSClass.h"
#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_ESPWebserver.h"

#define WEB_SERVER_DBG_PORT         SERIAL_PORT
#define WEB_FOTA_TAG_CONSOLE(...)   CONSOLE_TAG_LOGI("[WEB_OTA]", __VA_ARGS__)
#define WEB_FS_TAG_CONSOLE(...)     CONSOLE_TAG_LOGI("[WEB_FS]", __VA_ARGS__)
#if (WEB_SERVER_CONSOLE_ENABLE == 1)
#define WEB_SERVER_DBG_PRINTF(...)  CONSOLE_LOGI(__VA_ARGS__)
#define WEB_SERVER_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[WEB_Server]", __VA_ARGS__)
#else
#define WEB_SERVER_DBG_PRINTF(...)
#define WEB_SERVER_TAG_CONSOLE(...)
#endif

#define WEB_TAG_LOG(...)            FS_TAG_LOGI("[WEB]", __VA_ARGS__)

#define WEB_SERVER_START_TIMEOUT_MS 10000U // 10 seconds

// #include "log_pump.h"
extern boolean pumpLogTotalEnterCritical(const char* path);
extern boolean pumpLogTotalExitCritical(const char* path);

static WebServerURLHandleCallbacks defaultCallbacks;

class RedirectUrlHandler : public AsyncWebHandler
{
public:
  RedirectUrlHandler() {}
  virtual ~RedirectUrlHandler() {}
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
  bool canHandle(AsyncWebServerRequest *request) const override
#else
  bool canHandle(AsyncWebServerRequest *request) override
#endif
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

ESPWebServer::ESPWebServer(/* args */) : _isStarted(false)
{
}

ESPWebServer::~ESPWebServer()
{
  this->end();
}

#if (WEB_SERVER_REDIRECT_ENABLE == 1)
AsyncWebServer ESPWebServer::_server80(80);
#endif
AsyncWebServer ESPWebServer::_server(80);
ESPWebsocket* ESPWebServer::_wsHandler = nullptr;
std::vector<WebServerURLHandleCallbacks*> ESPWebServer::_pUrlCallbacks = {};
asyncHttpHandler ESPWebServer::_httpGetAuthHandler = nullptr;
asyncHttpHandler ESPWebServer::_httpGetHandler = nullptr;
asyncHttpHandler ESPWebServer::_httpPostAuthHandler = nullptr;
AsyncRateLimitMiddleware ESPWebServer::_rateLimitSpiffs;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
AsyncRateLimitMiddleware ESPWebServer::_rateLimitSdCard;
#endif
String ESPWebServer::_adminAuthUser = String();
String ESPWebServer::_adminAuthPass = String();
String ESPWebServer::_userAuthUser = String();
String ESPWebServer::_userAuthPass = String();
int ESPWebServer::_flashUpdateType = 0;
size_t ESPWebServer::_updateProgress = 0;
uint32_t ESPWebServer::_spiffsUploadPercent = 0;
uint32_t ESPWebServer::_flashUpdatePercent = 0;
boolean ESPWebServer::_IsUpdate = false;

void ESPWebServer::onUrlHandle(WebServerURLHandleCallbacks* pCallbacks)
{
  if (pCallbacks != nullptr)
  {
    _pUrlCallbacks.push_back(pCallbacks);
  }
}

#if (0)
void ESPWebServer::fs_editor_status(AsyncWebServerRequest *request)
{
  uint64_t totalBytes, usedBytes;
  char buf_ttb[64];
  char buf_udb[64];
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

  sprintf(buf_ttb,"%llu", totalBytes);
  sprintf(buf_udb,"%llu", usedBytes);
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
#endif

uint8_t ESPWebServer::authentication_level(AsyncWebServerRequest *request)
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

#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
void ESPWebServer::authenticationCallBack(AsyncWebServerRequest* request, ArMiddlewareNext next) {
  if (authentication_level(request) != HTTP_AUTH_FAIL) {
    next();
  }
}
#endif

void ESPWebServer::updatePrintProgress(size_t prg, size_t sz)
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
    WEB_FOTA_TAG_CONSOLE("Progress: %u%%", _flashUpdatePercent);
    snprintf(p, 5, "%u", _flashUpdatePercent);
    _wsHandler->eventsSend(p, "dfu");
  }

  /* Watch dog timer feed */
  wdt_reset();
}

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
uint32_t ESPWebServer::_sdUploadPercent = 0;
void ESPWebServer::sdfsPrintProgress(size_t prg, size_t sz)
{
  if (0 == sz) return;
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
    WEB_FS_TAG_CONSOLE("[SDFS] Progress: %u%%", _sdUploadPercent);
    snprintf(p, 5, "%u", _sdUploadPercent);
    _wsHandler->eventsSend(p, "sdfs");
  }
  /* Watch dog timer feed */
  wdt_reset();
}
#endif

void ESPWebServer::spiffsPrintProgress(size_t prg, size_t sz)
{
  if (0 == sz) return;
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
    WEB_FS_TAG_CONSOLE("[LITTLEFS] Progress: %u%%", _spiffsUploadPercent);
    snprintf(p, 5, "%u", _spiffsUploadPercent);
    _wsHandler->eventsSend(p, "spiffs");
  }
  /* Watch dog timer feed */
  wdt_reset();
}

void ESPWebServer::syncSsidNetworkToEvents()
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
    /* Esp32 dedicated an Event for scan network */
#endif
  }
}

void ESPWebServer::printHandleRequest(AsyncWebServerRequest *request)
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
  for (size_t i = 0; i < request->params(); i++)
  {
    const AsyncWebParameter* p = request->getParam(i);
    message += " NAME:" + p->name() + "\n VALUE:" + p->value() + "\n";
  }
  request->send(200, "text/html", message);
  WEB_SERVER_TAG_CONSOLE("[handleRequest] %s", message.c_str());
}

void ESPWebServer::begin(ESPWebsocket* ws)
{
  _wsHandler = ws;
  _adminAuthUser = ESPConfig.authAdminUser();
  _adminAuthPass = ESPConfig.authAdminPass();
  _userAuthUser = ESPConfig.authUserUser();
  _userAuthPass = ESPConfig.authUserPass();
  
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
      elem->onScanNetwork(std::bind(&ESPWebServer::syncSsidNetworkToEvents, this));
      using namespace std::placeholders;
      elem->onEventSocket(std::bind(&ESPWebsocket::eventsSend, _wsHandler, _1, _2));
    }
  }

  _server.addHandler(&_wsHandler->asyncEvents());
  _server.addHandler(&_wsHandler->asyncWs());

  // uri handle = "/edit"
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
  littleFsEditor.addMiddleware(&authenticationCallBack);
  // W5: real budget = EDITOR_DOWNLOAD_COUNTER_LIMIT requests per EDITOR_DOWNLOAD_COUNTER_RESET_TIMEOUT/1000 s (20 per 10 s)
  _rateLimitSpiffs.setMaxRequests(EDITOR_DOWNLOAD_COUNTER_LIMIT);
  _rateLimitSpiffs.setWindowSize(EDITOR_DOWNLOAD_COUNTER_RESET_TIMEOUT / 1000);
  littleFsEditor.addMiddleware(&_rateLimitSpiffs);
#else
  littleFsEditor.onAuthenticate([](AsyncWebServerRequest *request){
    return (authentication_level(request) != HTTP_AUTH_FAIL);
  });
#endif
  littleFsEditor.onProgress(spiffsPrintProgress);
  // littleFsEditor.onStatus(fs_editor_status);
  _server.addHandler(&littleFsEditor);

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  // uri handle = "/edit_sdfs"
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
  sdCardEditor.addMiddleware(&authenticationCallBack);
  // W5: real budget = EDITOR_DOWNLOAD_COUNTER_LIMIT requests per EDITOR_DOWNLOAD_COUNTER_RESET_TIMEOUT/1000 s (20 per 10 s)
  _rateLimitSdCard.setMaxRequests(EDITOR_DOWNLOAD_COUNTER_LIMIT);
  _rateLimitSdCard.setWindowSize(EDITOR_DOWNLOAD_COUNTER_RESET_TIMEOUT / 1000);
  sdCardEditor.addMiddleware(&_rateLimitSdCard);
#else
  sdCardEditor.onAuthenticate([](AsyncWebServerRequest *request){
    return (authentication_level(request) != HTTP_AUTH_FAIL);
  });
#endif
  sdCardEditor.onProgress(sdfsPrintProgress);
#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1) 
  sdCardEditor.onSemaphoreTake(pumpLogTotalEnterCritical);
  sdCardEditor.onSemaphoreGive(pumpLogTotalExitCritical);
#endif
  // sdCardEditor->onStatus(fs_editor_status);
  _server.addHandler(&sdCardEditor);
#endif

  _server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
          {
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
            if (authentication_level(request) != HTTP_AUTH_FAIL)
#endif
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
          })
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
          .addMiddleware(&authenticationCallBack);
#else
          ;
#endif

  _server.on("/get_open", HTTP_GET,
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
                  hasHandler = true;
                  break;
                }
              }

              if (!hasHandler)
              {
                printHandleRequest(request);
              }
            }
          });

  _server.on("/post", HTTP_POST, 
          [](AsyncWebServerRequest *request)
          {
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
            if (authentication_level(request) != HTTP_AUTH_FAIL)
#endif
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
          })
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
          .addMiddleware(&authenticationCallBack);
#else
          ;
#endif
  
#if (FIRMWARE_RELEASE_ACTIVATED == 1)
/* Handling Content-Type: raw (application/json) */
  _server.on("/petro_counter", HTTP_POST, 
          [](AsyncWebServerRequest *request)
          {
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
            if(authentication_level(request) != HTTP_AUTH_FAIL)
#endif
            {
              /* Combination TL and MT */
              const char resp[] PROGMEM = "{\"Data_type\": 2,\"Error_code\": 0,\"code\": 0,\"data\": {\"dataType\": 2}}";
              AsyncWebServerResponse *response = request->beginResponse(200, "text/json", resp);
              request->send(response);
            } 
          }, nullptr, 
          [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
          {
            if(!index)
            {
              WEB_SERVER_TAG_CONSOLE("BodyStart: %u", total);
            }
#if (WEB_SERVER_CONSOLE_ENABLE == 1)
            WEB_SERVER_DBG_PORT.println("[WEB] Data: ");
            WEB_SERVER_DBG_PORT.write(data, len);
            WEB_SERVER_DBG_PORT.println();
#endif
            if((index + len) == total)
            {
              WEB_SERVER_TAG_CONSOLE("BodyEnd: %u", total);
            } 
          })
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
          .addMiddleware(&authenticationCallBack);
#else
          ;
#endif

  /* Handling Content-Type: raw (application/json) */
  _server.on("/petro_status", HTTP_POST, [](AsyncWebServerRequest *request){
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
    if(authentication_level(request) != HTTP_AUTH_FAIL)
#endif
    {
      /* Combination TL and MT */
      const char resp[] PROGMEM = "{\"Data_type\": 1,\"Error_code\": 0,\"code\": 0,\"data\": {\"dataType\": 1}}";
      AsyncWebServerResponse *response = request->beginResponse(200, "text/json", resp);
      request->send(response);
    }
  }, nullptr
  , [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
    {
      WEB_SERVER_TAG_CONSOLE("BodyStart: %u", total);
    }
#if (WEB_SERVER_CONSOLE_ENABLE == 1)
    WEB_SERVER_DBG_PORT.println("[WEB] Data: ");
    WEB_SERVER_DBG_PORT.write(data, len);
    WEB_SERVER_DBG_PORT.println();
#endif
    if((index + len) == total)
    {
      WEB_SERVER_TAG_CONSOLE("BodyEnd: %u", total);
    }
  })
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
  .addMiddleware(&authenticationCallBack);
#else
  ;
#endif
#endif // #if (FIRMWARE_RELEASE_ACTIVATED == 1)

  _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    uint8_t level = authentication_level(request);
    switch (level) {
      case HTTP_AUTH_LV0:
        request->send(NAND_FS_SYSTEM, "/home.htm");
      break;

      case HTTP_AUTH_LV1:
        request->send(NAND_FS_SYSTEM, "/home_user.htm");
      break;

      default:
      break;
    }
  })
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
    .addMiddleware(&authenticationCallBack);
#else
    ;
#endif

  /* Serving files in directory. Serving static files with authentication */
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
  _server.serveStatic("/", NAND_FS_SYSTEM, "/").addMiddleware(&authenticationCallBack);
#else
  _server.serveStatic("/", NAND_FS_SYSTEM, "/").onAuthenticate([](AsyncWebServerRequest *request){
    return (authentication_level(request) != HTTP_AUTH_FAIL);
  });
#endif

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
  if (SDCard.statusIsOk()) {
    _server.serveStatic("/onsd", SD_FS_SYSTEM, "/").addMiddleware(&authenticationCallBack);
  } else {
    _server.serveStatic("/onsd", NAND_FS_SYSTEM, "/").addMiddleware(&authenticationCallBack);
  }
#else
  if (SDCard.statusIsOk()) {
    _server.serveStatic("/onsd", SD_FS_SYSTEM, "/").onAuthenticate([](AsyncWebServerRequest *request){
      return (authentication_level(request) != HTTP_AUTH_FAIL);
    });
  } else {
    _server.serveStatic("/onsd", NAND_FS_SYSTEM, "/").onAuthenticate([](AsyncWebServerRequest *request){
      return (authentication_level(request) != HTTP_AUTH_FAIL);
    });
  }
#endif
#endif

  // Simple Firmware Update Form
  _server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
             {
               if (authentication_level(request) == HTTP_AUTH_LV0)
               {
                 request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
               }
             });
  _server.on(
      "/update", HTTP_POST, [](AsyncWebServerRequest *request)
      {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", !Update.hasError() ? "Update OK" : "Update FAIL");
        /* 08/02/2025
          E (5640550) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
          E (5640550) task_wdt:  - asyncTcpSock (CPU 0/1)
          E (5640550) task_wdt: Tasks currently running:
          E (5640550) task_wdt: CPU 0: IDLE0
          E (5640550) task_wdt: CPU 1: loopTask
          */
        esp_task_wdt_reset();
        response->addHeader("Connection", "close");
        request->send(response);
        _IsUpdate = false;
        if (!request->hasArg("ota_event")) {
          ESPWifi.OTAStop();
        }
        if (!Update.hasError())
        {
          updatePrintProgress(100, 100); // 100% --> Done
          WEB_TAG_LOG("[OTA] Succeed");
          if (U_FLASH == _flashUpdateType)
          {
            ESPWifi.OTASuccess();
            SOFTReset.enable(1000, ESPSoftReset::OTA_UPDATE_TYPE);
          }
          else
          {
            /* Save current configure */
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

          if (!request->hasArg("ota_event")) {
            ESPWifi.OTAStart();
          }
          _IsUpdate = true;
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
#if (WEB_SERVER_CONSOLE_ENABLE == 1)
            Update.printError(WEB_SERVER_DBG_PORT);
#endif
            _IsUpdate = false;
          }
        }

        if (!Update.hasError())
        {
          /* 08/02/2025
            E (5640550) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
            E (5640550) task_wdt:  - asyncTcpSock (CPU 0/1)
            E (5640550) task_wdt: Tasks currently running:
            E (5640550) task_wdt: CPU 0: IDLE0
            E (5640550) task_wdt: CPU 1: loopTask
            */
          esp_task_wdt_reset();
          if (Update.write(data, len) != len)
          {
#if (WEB_SERVER_CONSOLE_ENABLE == 1)
            Update.printError(WEB_SERVER_DBG_PORT);
#endif
          }
          _updateProgress += len;
          updatePrintProgress(_updateProgress, length);
        } else {
          _IsUpdate = false;
        }

        if (!_IsUpdate) {
          ESPWifi.OTAStop();
          Update.end(true); // End update with error
          updatePrintProgress(101, 100); // 101% --> Fail
          WEB_TAG_LOG("[OTA] Failed");
          request->abort();
        }

        if (final) { 
          /* 08/02/2025
            E (5640550) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
            E (5640550) task_wdt:  - asyncTcpSock (CPU 0/1)
            E (5640550) task_wdt: Tasks currently running:
            E (5640550) task_wdt: CPU 0: IDLE0
            E (5640550) task_wdt: CPU 1: loopTask
            */
          esp_task_wdt_reset();
          if (Update.end(true))
          {
            WEB_SERVER_TAG_CONSOLE("Update Success: %uB", index + len);
          }
#if (WEB_SERVER_CONSOLE_ENABLE == 1)
          else
          {
            Update.printError(WEB_SERVER_DBG_PORT);
          }
#endif
        }
      });

  _server.onNotFound(
      [](AsyncWebServerRequest *request)
      {
#if (WEB_SERVER_CONSOLE_ENABLE == 1)
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
        size_t i;
        for (i = 0; i < headers; i++)
        {
          const AsyncWebHeader *h = request->getHeader(i);
          WEB_SERVER_TAG_CONSOLE("_HEADER[%s]: %s", h->name().c_str(), h->value().c_str());
        }

        int params = request->params();
        for (i = 0; i < params; i++)
        {
          const AsyncWebParameter *p = request->getParam(i);
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
#endif
        request->send(404);
      });

#if (WEB_SERVER_CONSOLE_ENABLE == 1)
  _server.onFileUpload(
      [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
      {
        if (!index)
        {
          WEB_SERVER_TAG_CONSOLE("UploadStart: %s", filename.c_str());
        }
#if (WEB_SERVER_CONSOLE_ENABLE == 1)
        WEB_SERVER_DBG_PORT.write(data, len);
#endif
        if (final)
        {
          WEB_SERVER_TAG_CONSOLE("UploadEnd: %s (%u)", filename.c_str(), index + len);
        }
      });

  _server.onRequestBody(
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
        if (!index)
        {
          WEB_SERVER_TAG_CONSOLE("BodyStart: %u", total);
        }
#if (WEB_SERVER_CONSOLE_ENABLE == 1)
        WEB_SERVER_DBG_PORT.write(data, len);
#endif
        if (index + len == total)
        {
          WEB_SERVER_TAG_CONSOLE("BodyEnd: %u", total);
        }
      });
#endif

  /* Protect server port don't less 80 */
  if (ESPConfig.tcpPort() < 80) {
    // set server port default
    ESPConfig.tcpPortSet(SERVER_PORT_DEFAULT);
  }

  if (!EEPParams.isDeviceActivated()) {
    start();
  } else {
    // Start web server once after 15 seconds
    // This is needed to ensure that the WiFi connection is established before starting the server.
    WEB_TAG_LOG("Web Server will start after %u seconds", WEB_SERVER_START_TIMEOUT_MS / 1000);
    // Use a ticker to delay the start of the server
    allocate_ticker_once_ms(WEB_SERVER_START_TIMEOUT_MS, [](void* arg)
    {
      ESPWebServer* pThis = static_cast<ESPWebServer*>(arg);
      if (pThis) {
        pThis->start();
      }
    }, this);
  }
}

void ESPWebServer::start() {
  if (_isStarted) {
    WEB_SERVER_TAG_CONSOLE("Web Server already started");
    return;
  }
  _isStarted = true;
  
#if (WEB_SERVER_REDIRECT_ENABLE == 1)
  /* redirect port 80 to tcp port */
  if (ESPConfig.tcpPort() != 80) {
    _server80.addHandler(new RedirectUrlHandler());
    _server80.begin(80);
  }
#endif
  
  WEB_SERVER_TAG_CONSOLE("Starting Web Server Port: %u", ESPConfig.tcpPort());
  /* Start web server */
  if (_server.begin(ESPConfig.tcpPort())) {
    WEB_TAG_LOG("Web Server started on port %u", ESPConfig.tcpPort());
  } else {
    WEB_TAG_LOG("Web Server failed to start on port %u", ESPConfig.tcpPort());
  }
}

void ESPWebServer::loop()
{

}

WebServerURLHandleCallbacks::WebServerURLHandleCallbacks() 
: _pScanNetworkCb(nullptr)
{
}
WebServerURLHandleCallbacks::~WebServerURLHandleCallbacks() {}
/**
 * 
 * Handler called after once request with method GET and authenticated.
 */
int WebServerURLHandleCallbacks::onHttpGetAuth(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[WebServerURLHandleCallbacks] >> onHttpGetAuth: default <<");
  return -1;
}
/**
 * Handler called after once request with method GET.
 */
int WebServerURLHandleCallbacks::onHttpGet(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[WebServerURLHandleCallbacks] >> onHttpGet: default <<");
  return -1;
}
/**
 * Handler called after once request with method POST and authenticated.
 */
int WebServerURLHandleCallbacks::onHttpPostAuth(AsyncWebServerRequest *request)
{
  WEB_SERVER_TAG_CONSOLE("[WebServerURLHandleCallbacks] >> onHttpPostAuth: default <<");
  return -1;
}
