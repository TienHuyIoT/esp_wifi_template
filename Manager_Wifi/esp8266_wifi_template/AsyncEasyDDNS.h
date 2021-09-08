/*
EasyDDNS Library for ESP8266 or ESP32
See the README file for more details.

Written in 2017 by Ayush Sharma. Licensed under MIT.
*/

#ifndef _ASYNC_EASY_DDNS_H
#define _ASYNC_EASY_DDNS_H

#include <Arduino.h>

#include <asyncHTTPrequest.h>

#define ASYNC_EASYDDNS_DEBUG    1

// Handler to notify user about new public IP
typedef std::function<void(const char* old_ip, const char* new_ip)> AsyncDDNSUpdateHandler;

class AsyncEasyDDNSClass{
public:
  AsyncEasyDDNSClass();
  asyncHTTPrequest request_get_ip;
  asyncHTTPrequest request_post_ip;
  IPAddress ddnsip;
  void service(String ddns_service);
  void client(String ddns_domain, String ddns_username, String ddns_password = "");
  void update();  

  // Callback
  void onUpdate(AsyncDDNSUpdateHandler handler) {
    _ddnsUpdateFunc = handler;
  }

private:
  AsyncDDNSUpdateHandler _ddnsUpdateFunc = nullptr;
  String new_ip;
  String old_ip;
  String ddns_u;
  String ddns_p;
  String ddns_d;
  String ddns_choice;
  String base64Authorization;
  void run_get_ip();
  void run_post_ip();
  void request_get_ip_cb(void* optParm, asyncHTTPrequest* request, int readyState);
  void request_post_ip_cb(void* optParm, asyncHTTPrequest* request, int readyState);
};
extern AsyncEasyDDNSClass AsyncEasyDDNS;

#endif // _ASYNC_EASY_DDNS_H
