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
*/

#ifndef __ESP_ASYNC_EASY_DDNS_H
#define __ESP_ASYNC_EASY_DDNS_H

#include <Arduino.h>
#include <functional>
#include <asyncHTTPrequest.h>

#define ASYNC_EASYDDNS_DEBUG    1

// Handler to notify user about new public IP
typedef std::function<void(const char* oldIP, const char* newIP)> AsyncDDNSUpdateHandler;

class ESPAsyncEasyDDNS{
public:
  ESPAsyncEasyDDNS();
  IPAddress ddnsIP;
  void service(String ddns_service);
  void begin(String ddns_domain, String ddns_username, String ddns_password = "");
  void update();  

  // Callback
  void onUpdateIP(AsyncDDNSUpdateHandler handler) {
    _ddnsUpdateFunc = handler;
  }

private:
  AsyncDDNSUpdateHandler _ddnsUpdateFunc = nullptr;
  asyncHTTPrequest _aHttpGetIP;
  asyncHTTPrequest _aHttpPostIP;
  String _newIP;
  String _oldIP;
  String _userName;
  String _passWord;
  String _domain;
  String _serverDDNS;
  String _base64Authorization;
  void getIP();
  void postIP();
  void getIpCallback(void* optParm, asyncHTTPrequest* request, int readyState);
  void postIpCallback(void* optParm, asyncHTTPrequest* request, int readyState);
};

#endif // __ESP_ASYNC_EASY_DDNS_H
