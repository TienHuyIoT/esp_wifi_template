#ifndef	__ESP_WEB_SERVER_H
#define __ESP_WEB_SERVER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include <ESPAsyncWebServer.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_APPConfig.h"
#include "THIoT_ESPFSEditor.h"
#include "THIoT_ESPFSHandle.h"
#include "THIoT_ESPWebsocket.h"
#include "THIoT_ESPWsDataHandler.h"
#include "THIoT_PFSerialTrace.h"

#if (gConsoleDebugLevel > 0)
#define WEB_SERVER_CONSOLE_ENABLE 0
#else
#define WEB_SERVER_CONSOLE_ENABLE 0
#endif

typedef enum {
  HTTP_AUTH_LV0 = 0,
  HTTP_AUTH_LV1,
  HTTP_AUTH_LV2,
  HTTP_AUTH_FAIL
} http_auth_level_t;

typedef std::function<void(AsyncWebServerRequest *)> asyncHttpHandler;

typedef std::function<void(void)> scanNetworkHandler;
typedef std::function<void(const char *, const char *)> eventSocketHandler;

class WebServerURLHandleCallbacks {
private:
  scanNetworkHandler _pScanNetworkCb;
  eventSocketHandler _pEventSocketCb;
public:
  WebServerURLHandleCallbacks();
	virtual ~WebServerURLHandleCallbacks();
  /**
  * Handler called after once request with method GET and authenticated.
  */
  virtual int onHttpGetAuth(AsyncWebServerRequest* request);
  /**
    * Handler called after once request with method GET.
    */
  virtual int onHttpGet(AsyncWebServerRequest* request);
  /**
    * Handler called after once request with method POST and authenticated.
    */
  virtual int onHttpPostAuth(AsyncWebServerRequest* request);

  /**
   * A class derived will call this function to active async scan network
   * 
  */
  virtual void asyncScanNetwork()
  {
    if (_pScanNetworkCb)
    {
      _pScanNetworkCb();
    }
  }

  /**
   * A class derived will call this function to send async event socket
   * 
  */
  virtual void eventsSend(const char *message, const char *event)
  {
    if (_pEventSocketCb)
    {
      _pEventSocketCb(message, event);
    }
  }

  /**
   * Register an async scan network function
  */
  void onScanNetwork(scanNetworkHandler cb)
  {
    _pScanNetworkCb = cb;
  }

  /**
   * Register an async event socket function
  */
  void onEventSocket(eventSocketHandler cb)
  {
    _pEventSocketCb = cb;
  }
};

class ESPWebServer
{
private:
  static constexpr uint16_t SERVER_PORT_DEFAULT = 25123;
#if (WEB_SERVER_REDIRECT_ENABLE == 1)
  static AsyncWebServer _server80;
#endif
  static AsyncWebServer _server;
  static AsyncRateLimitMiddleware _rateLimitSpiffs;
  static ESPWebsocket* _wsHandler;
  static std::vector<WebServerURLHandleCallbacks*> _pUrlCallbacks;
  static asyncHttpHandler _httpGetAuthHandler;
  static asyncHttpHandler _httpGetHandler;
  static asyncHttpHandler _httpPostAuthHandler;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  static AsyncRateLimitMiddleware _rateLimitSdCard;
  static uint32_t _sdUploadPercent;
#endif
  static String _adminAuthUser;
  static String _adminAuthPass;
  static String _userAuthUser;
  static String _userAuthPass;
  static int _flashUpdateType;
  static uint32_t _spiffsUploadPercent;
  static uint32_t _flashUpdatePercent;
  static size_t _updateProgress;
  static boolean _IsUpdate;
  bool _isStarted;
  String _uriHttpGetAuth;
  String _uriHttpGet;
  String _uriHttpPostAuth;

  static uint8_t authentication_level(AsyncWebServerRequest *request);
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
  static void authenticationCallBack(AsyncWebServerRequest* request, ArMiddlewareNext next);
#endif
  static void updatePrintProgress(size_t prg, size_t sz);
  static void spiffsPrintProgress(size_t prg, size_t sz);
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  static void sdfsPrintProgress(size_t prg, size_t sz);
#endif
  // static void fs_editor_status(AsyncWebServerRequest *request);
  static void printHandleRequest(AsyncWebServerRequest *request);

public:
  ESPWebServer(/* args */);
  ~ESPWebServer();

  void begin(ESPWebsocket* ws);
  void start();
  void loop();

  boolean IsUpdate() { return _IsUpdate; }
  void UpdateStart() { _IsUpdate = true; }
  void UpdateStop() { _IsUpdate = false; }

  bool editorIsDownloadLimited() {
    bool status;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
    status = sdCardEditor.isDownloadLimited();
#else
    status = littleFsEditor.isDownloadLimited();
#endif
    return status;
  }

  void editorDownloadLimited(bool limit) {
    littleFsEditor.downloadLimited(limit);
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
    sdCardEditor.downloadLimited(limit);
#endif
  }

  bool editorIsLock() {
    bool status;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
    status = sdCardEditor.isLockFile();
#else
    status = littleFsEditor.isLockFile();
#endif
    return status;
  }

  void editorLock(bool isLock) {
    if (isLock) {
      littleFsEditor.lockFile();
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
      sdCardEditor.lockFile();
#endif
    } else {
      littleFsEditor.unlockFile();
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
      sdCardEditor.unlockFile();
#endif
    }
  }
  
  void onHttpGetAuth(asyncHttpHandler handler, const char* uri = "/get")
  {
    _httpGetAuthHandler = handler;
    _uriHttpGetAuth = uri;
  }

  void onHttpGet(asyncHttpHandler handler, const char* uri = "/get_open")
  {
    _httpGetHandler = handler;
    _uriHttpGet = uri;
  }

  void onHttpPostAuth(asyncHttpHandler handler, const char* uri = "/post")
  {
    _httpPostAuthHandler = handler;
    _uriHttpPostAuth = uri;
  }

  void onUrlHandle(WebServerURLHandleCallbacks* pCallbacks);

  void syncSsidNetworkToEvents();

  void eventsSend(const char *message, const char *event)
  {
    _wsHandler->eventsSend(message, event);
  }

  void end() {
    _wsHandler = nullptr;
  }
};

#endif // __ESP_WEB_SERVER_H
