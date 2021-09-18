#ifndef	_ASYNC_WEB_SERVER_H
#define _ASYNC_WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "app_config.h"
#include "hth_fs_editor.h"
#include "hth_fs_handle.h"
#include "hth_websocket.h"
#include "hth_websocket_payload.h"

typedef enum {
  HTTP_AUTH_LV0 = 0,
  HTTP_AUTH_LV1,
  HTTP_AUTH_LV2,
  HTTP_AUTH_FAIL
} http_auth_level_t;

typedef std::function<void(AsyncWebServerRequest *)> asyncHttpHandler;

typedef std::function<void(void)> scanNetworkHandler;

class serverCallbacks {
private:
  scanNetworkHandler _pScanNetworkCb;
public:
  serverCallbacks();
	virtual ~serverCallbacks();
  /**
  * Handler called after once request with method GET and authenticated.
  */
  virtual void onHttpGetAuth(AsyncWebServerRequest* request);
  /**
    * Handler called after once request with method GET.
    */
  virtual void onHttpGet(AsyncWebServerRequest* request);
  /**
    * Handler called after once request with method POST and authenticated.
    */
  virtual void onHttpPostAuth(AsyncWebServerRequest* request);

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
   * Register an async scan network function
  */
  void onScanNetwork(scanNetworkHandler cb)
  {
    _pScanNetworkCb = cb;
  }
};

class async_webserver
{
private:
  static AsyncWebServer* _server;
  static AsyncWebServer* _server80;
  static FSEditor* _spiffsEditor;
  static async_websocket* _wsHandler;
  static serverCallbacks* _pCallbacks;
  static asyncHttpHandler _httpGetAuthHandler;
  static asyncHttpHandler _httpGetHandler;
  static asyncHttpHandler _httpPostAuthHandler;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  static FSEditor* _sdCardEditor;
  static uint32_t _sdUploadPercent;
#endif
  static String _adminAuthUser;
  static String _adminAuthPass;
  static String _userAuthUser;
  static String _userAuthPass;
  static int _flashUpdateType;
  static uint32_t _spiffsUploadPercent;
  static uint32_t _flashUpdatePercent;
#ifdef ESP8266
  static size_t _updateProgress;
#endif
  String _uriHttpGetAuth;
  String _uriHttpGet;
  String _uriHttpPostAuth;

  static uint8_t authentication_level(AsyncWebServerRequest *request);
  static void updatePrintProgress(size_t prg, size_t sz);
  static void spiffsPrintProgress(size_t prg, size_t sz);
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  static void sdfsPrintProgress(size_t prg, size_t sz);
#endif
  static void fs_editor_status(AsyncWebServerRequest *request);

public:
  async_webserver(/* args */);
  ~async_webserver();

  void begin();
  void loop();
  
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

  void setHandleCallbacks(serverCallbacks* pCallbacks);

  void syncSsidNetworkToEvents();

  void end() {
    delete _server;
    delete _server80;
    if (_spiffsEditor)
    {
      delete _spiffsEditor;
    }
    delete _wsHandler;
    delete _pCallbacks;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
    if (_sdCardEditor)
    {
      delete _sdCardEditor;
    }
#endif
  }
};

#endif
