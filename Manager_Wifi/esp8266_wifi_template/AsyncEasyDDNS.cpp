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

#include <Arduino.h>
#include <base64.h>
#include "AsyncEasyDDNS.h"

#if (defined ASYNC_EASYDDNS_DEBUG) && (ASYNC_EASYDDNS_DEBUG == 1)
#include "hth_console_dbg.h"
#define ASYNC_EASYDDNS_DBG_PORT CONSOLE_PORT
#define ASYNC_EASYDDNS_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define ASYNC_EASYDDNS_TAG_PRINTF(...) CONSOLE_TAG_LOGI("[EASYDDNS]", __VA_ARGS__)
#define ASYNC_EASYDDNS_GET_PRINTF(...) CONSOLE_TAG_LOGI("[EASYDDNS GET]", __VA_ARGS__)
#define ASYNC_EASYDDNS_POST_PRINTF(...) CONSOLE_TAG_LOGI("[EASYDDNS POST]", __VA_ARGS__)
#else
#define ASYNC_EASYDDNS_PRINTF(f_, ...)
#define ASYNC_EASYDDNS_TAG_PRINTF(...)
#define ASYNC_EASYDDNS_GET_PRINTF(f_, ...)
#define ASYNC_EASYDDNS_POST_PRINTF(f_, ...)
#endif

AsyncEasyDDNSClass::AsyncEasyDDNSClass()
{
  using namespace std::placeholders;  
  ddnsip.fromString("0.0.0.0");
  ddns_choice = "";
  ddns_d = "";
  ddns_u = "";
  ddns_p = "";
  request_get_ip.setDebug(false);  
  request_post_ip.setDebug(false); 
  request_get_ip.onReadyStateChange(std::bind(&AsyncEasyDDNSClass::request_get_ip_cb, this, _1, _2, _3));
  request_post_ip.onReadyStateChange(std::bind(&AsyncEasyDDNSClass::request_post_ip_cb, this, _1, _2, _3));
}

void AsyncEasyDDNSClass::service(String ddns_service) {
  ddns_choice = ddns_service;
}

void AsyncEasyDDNSClass::client(String ddns_domain, String ddns_username, String ddns_password) {
  ddns_d = ddns_domain;
  ddns_u = ddns_username;
  ddns_p = ddns_password;
}

void AsyncEasyDDNSClass::request_get_ip_cb(void* optParm, asyncHTTPrequest* request, int readyState) 
{
  if (readyState == 4) 
  {
    new_ip = request->responseText();
    ASYNC_EASYDDNS_TAG_PRINTF("**************************************");
    ASYNC_EASYDDNS_TAG_PRINTF("new_ip: %s", new_ip.c_str());
    ASYNC_EASYDDNS_TAG_PRINTF("**************************************");    
    request->setDebug(false);
    run_post_ip();    
  }
}

void AsyncEasyDDNSClass::request_post_ip_cb(void* optParm, asyncHTTPrequest* request, int readyState) 
{
  if (readyState == 4) 
  {
    ASYNC_EASYDDNS_TAG_PRINTF("**************************************");
    ASYNC_EASYDDNS_DBG_PORT.print(request->responseText());      
    request->setDebug(false);
    ddnsip.fromString(new_ip);
    if (old_ip != new_ip)
    {
      ASYNC_EASYDDNS_PRINTF("new_ip\r\n");      
      // Send a callback notification
      if(_ddnsUpdateFunc != nullptr){
        _ddnsUpdateFunc(old_ip.c_str(), new_ip.c_str());
      }
      // Replace Old IP with new one to detect further changes.
      old_ip = new_ip;
    }
    else
    {
      ASYNC_EASYDDNS_PRINTF("old_ip\r\n");
    } 
    ASYNC_EASYDDNS_TAG_PRINTF("**************************************");     
  }
}

void AsyncEasyDDNSClass::run_get_ip()
{
  ddnsip.fromString("0.0.0.0");
  if (request_get_ip.readyState() == 0 || request_get_ip.readyState() == 4)
  {
    bool result = request_get_ip.open("GET", "http://ipv4bot.whatismyipaddress.com/");
    
    if (result)
    {
      // Only send() if open() returns true, or crash
      request_get_ip.send();
    }
    else
    {
      ASYNC_EASYDDNS_GET_PRINTF("Can't send bad request");
    }
  }
  else
  {
    ASYNC_EASYDDNS_GET_PRINTF("Can't send request");
  }
}

void AsyncEasyDDNSClass::run_post_ip()
{
  if (request_post_ip.readyState() == 0 || request_post_ip.readyState() == 4)
  {
    String update_url = "";    
    uint8_t auth_service = 0;
    // ######## GENERATE UPDATE URL ######## //
    if (ddns_choice == "duckdns") {
      update_url = "http://www.duckdns.org/update?domains=" + ddns_d + "&token=" + ddns_u + "&ip=" + new_ip + "";
    } else if (ddns_choice == "noip") {
      auth_service = 1;
      update_url = "http://dynupdate.no-ip.com/nic/update?hostname=" + ddns_d + "&myip=" + new_ip + "";
    } else if (ddns_choice == "dyndns") {
      auth_service = 1;
      update_url = "http://members.dyndns.org/v3/update?hostname=" + ddns_d + "&myip=" + new_ip + "";
    } else if (ddns_choice == "dynu") {
      update_url = "http://api.dynu.com/nic/update?hostname=" + ddns_d + "&myip=" + new_ip + "&username=" + ddns_u + "&password=" + ddns_p + "";
    } else if (ddns_choice == "enom") {
      update_url = "http://dynamic.name-services.com/interface.asp?command=SetDnsHost&HostName=" + ddns_d + "&Zone=" + ddns_u + "&DomainPassword=" + ddns_p + "&Address=" + new_ip + "";
    } else if (ddns_choice == "all-inkl") {
      auth_service = 1;
      update_url = "http://dyndns.kasserver.com/?myip=" + new_ip;
    } else if (ddns_choice == "selfhost.de") {
      auth_service = 1;
      update_url = "http://carol.selfhost.de/nic/update?";
    } else if (ddns_choice == "dyndns.it") {
      auth_service = 1;
      update_url = "http://update.dyndns.it/nic/update?hostname=" + ddns_d;
    } else if (ddns_choice == "strato") {
      auth_service = 1;
      update_url = "http://dyndns.strato.com/nic/update?hostname=" + ddns_d + "&myip=" + new_ip + "";
    } else if (ddns_choice == "freemyip") {
      update_url = "http://freemyip.com/update?domain=" + ddns_d + "&token=" + ddns_u + "&myip=" + new_ip + "";
    } else if (ddns_choice == "afraid.org") {
      update_url = "http://sync.afraid.org/u/" + ddns_u + "/";
    } else {
      ASYNC_EASYDDNS_POST_PRINTF("## INPUT CORRECT DDNS SERVICE NAME ##\r\n");
      return;
    }     

    ASYNC_EASYDDNS_POST_PRINTF("URL: %s", update_url.c_str());
    bool result = request_post_ip.open("GET", update_url.c_str());
    if(auth_service)
    {
      String auth = ddns_u + ":" + ddns_p;
      base64Authorization = base64::encode(auth);
      // ASYNC_EASYDDNS_POST_PRINTF("auth (%s)-(%s)", auth.c_str(), FPSTR(base64Authorization.c_str()));
      request_post_ip.setReqHeader("Authorization: Basic ", FPSTR(base64Authorization.c_str()));
    } 
    if (result)
    {
      // Only send() if open() returns true, or crash
      request_post_ip.send();
    }
    else
    {
      ASYNC_EASYDDNS_POST_PRINTF("Can't send bad request");
    }
  }
  else
  {
    ASYNC_EASYDDNS_POST_PRINTF("Can't send request");
  }
}

void AsyncEasyDDNSClass::update() 
{
  ASYNC_EASYDDNS_TAG_PRINTF("Update IP");
  // ######## GET PUBLIC IP ######## //
  run_get_ip();
}
