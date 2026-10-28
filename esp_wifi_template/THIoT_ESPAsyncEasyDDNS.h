/*
EasyDDNS Library for ESP8266 or ESP32
See the README file for more details.

Written in 2017 by Ayush Sharma. Licensed under MIT.
Upgrade AsyncEasyDDNS in 2021 by TienHuyIoT

Original: https://github.com/ayushsharma82/EasyDDNS

Some Library references:
- asyncHTTPrequest:
  https://github.com/boblemaire/asyncHTTPrequest

- ESPAsyncTCP for ESP8266
  https://github.com/me-no-dev/ESPAsyncTCP

- AsyncTCP for ESP32
  https://github.com/me-no-dev/AsyncTCP


Private DDNS

update_url = _serverDDNS + "/update?hostname=" + _domain + "&myip=" + _newIP
Ex: http://montech.ddns.net/update?hostname=cxauco.ddns.net&myip=27.64.65.61

*/

#ifndef __ESP_ASYNC_EASY_DDNS_H
#define __ESP_ASYNC_EASY_DDNS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <asyncHTTPrequest.h>
#include "THIoT_PFTicker.h"

#define ASYNC_EASYDDNS_DEBUG    0

// Handler to notify user about new public IP
typedef std::function<void(const char* oldIP, const char* newIP)> AsyncDDNSUpdateHandler;
typedef std::function<void(void *arg, JsonObject& jData)> AsyncDDNSPriDataHandler;

class ESPAsyncEasyDDNS{
public:
  enum : uint8_t {
    DDNS_SERVICE = 0,
    MON_SERVICE
  };
  ESPAsyncEasyDDNS();
  void server(String serviceDDNS);
  void service(String serviceDDNS);
  void urlGetIp(String url);
  void begin(String ddns_domain, String ddns_username, String ddns_password, int interval = 120, bool enable = false);
  void updateTrigger(uint16_t timeout = 15); 

  // Callback
  void onUpdateIP(AsyncDDNSUpdateHandler handler) {
    _ddnsUpdateFunc = handler;
  }

  void onPrivateData(AsyncDDNSPriDataHandler handler, void *arg = nullptr) {
    _priDataHandle = handler;
    _priDataHandleArg = arg;
  }

  String ddnsIP() { return _ddnsIP.toString(); }

private:
  AsyncDDNSUpdateHandler _ddnsUpdateFunc = nullptr;
  AsyncDDNSPriDataHandler _priDataHandle = nullptr;
  void *_priDataHandleArg;
  asyncHTTPrequest _aHttpGetIP;
  asyncHTTPrequest _aHttpPostIP;
  IPAddress _ddnsIP;
  ticker_function_handle_t _ddnsTicker;
  String _serverDDNS;
  String _newIP;
  String _oldIP;
  String _userName;
  String _passWord;
  String _domain;
  String _urlGetIp;
  String _serviceDDNS;
  String _base64Authorization;
  uint16_t _interval;
  boolean _isStart;
  boolean _updateStatus;
  void _start(uint16_t interval);
  void _update(); 
  void getPublicIP();
  void postIP2DynDNS();
  void postIP2Customer();
  void getIpCallback(void* optParm, asyncHTTPrequest* request, int readyState);
  void postIpCallback(void* optParm, asyncHTTPrequest* request, int readyState);
};

extern ESPAsyncEasyDDNS EasyDDNS;
extern ESPAsyncEasyDDNS MonTechDDNS;

#endif // __ESP_ASYNC_EASY_DDNS_H
