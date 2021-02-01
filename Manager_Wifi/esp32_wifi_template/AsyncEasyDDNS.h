/*
EasyDDNS Library for ESP8266 or ESP32
See the README file for more details.

Written in 2017 by Ayush Sharma. Licensed under MIT.
*/

#ifndef AsyncEasyDDNS_H
#define AsyncEasyDDNS_H

#include "Arduino.h"

#if (ESP8266)
  #include <ESP8266WiFi.h>
#elif (ESP32)
  #include <WiFi.h>
#endif

#include <AsyncHTTPRequest_Generic.h>

#define ASYNC_EASYDDNS_DEBUG    1

// Handler to notify user about new public IP
typedef std::function<void(const char* old_ip, const char* new_ip)> DDNSUpdateHandler;

class AsyncEasyDDNSClass{
public:
  AsyncEasyDDNSClass();
  AsyncHTTPRequest request_get_ip;
  AsyncHTTPRequest request_post_ip;
  void service(String ddns_service);
  void client(String ddns_domain, String ddns_username, String ddns_password = "");
  void update(unsigned long ddns_update_interval);
  void request_get_ip_cb(void* optParm, AsyncHTTPRequest* request, int readyState);
  void request_post_ip_cb(void* optParm, AsyncHTTPRequest* request, int readyState);

  // Callback
  void onUpdate(DDNSUpdateHandler handler) {
    _ddnsUpdateFunc = handler;
  }

private:
  DDNSUpdateHandler _ddnsUpdateFunc = nullptr;
  unsigned long interval;
  unsigned long previousMillis;
  
  String new_ip;
  String old_ip;
  String ddns_u;
  String ddns_p;
  String ddns_d;
  String ddns_choice;
  void run_get_ip();
  void run_post_ip();
};
extern AsyncEasyDDNSClass AsyncEasyDDNS;
#endif
