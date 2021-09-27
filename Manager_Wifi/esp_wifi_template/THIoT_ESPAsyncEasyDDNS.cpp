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
#include "THIoT_ESPAsyncEasyDDNS.h"

#if (defined ASYNC_EASYDDNS_DEBUG) && (ASYNC_EASYDDNS_DEBUG == 1)
#include "THIoT_SerialTrace.h"
#define EASYDDNS_PORT SERIAL_PORT
#define EASYDDNS_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#define EASYDDNS_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[EASYDDNS]", __VA_ARGS__)
#else
#define EASYDDNS_CONSOLE(f_, ...)
#define EASYDDNS_TAG_CONSOLE(...)
#endif

ESPAsyncEasyDDNS::ESPAsyncEasyDDNS()
{
  using namespace std::placeholders;  
  ddnsIP.fromString("0.0.0.0");
  _serverDDNS = "";
  _domain = "";
  _userName = "";
  _passWord = "";
  _ddnsUpdateFunc = nullptr;
  _aHttpGetIP.setDebug(false);  
  _aHttpPostIP.setDebug(false); 
  _aHttpGetIP.onReadyStateChange(std::bind(&ESPAsyncEasyDDNS::getIpCallback, this, _1, _2, _3));
  _aHttpPostIP.onReadyStateChange(std::bind(&ESPAsyncEasyDDNS::postIpCallback, this, _1, _2, _3));
}

void ESPAsyncEasyDDNS::service(String ddns_service) {
  _serverDDNS = ddns_service;
}

void ESPAsyncEasyDDNS::begin(String ddns_domain, String ddns_username, String ddns_password) {
  _domain = ddns_domain;
  _userName = ddns_username;
  _passWord = ddns_password;
}

void ESPAsyncEasyDDNS::getIpCallback(void* optParm, asyncHTTPrequest* request, int readyState) 
{
  if (readyState == 4) 
  {
    _newIP = request->responseText();
    EASYDDNS_TAG_CONSOLE("**************************************");
    EASYDDNS_TAG_CONSOLE("_newIP: %s", _newIP.c_str());
    EASYDDNS_TAG_CONSOLE("**************************************");    
    request->setDebug(false);
    postIP();    
  }
}

void ESPAsyncEasyDDNS::postIpCallback(void* optParm, asyncHTTPrequest* request, int readyState) 
{
  if (readyState == 4) 
  {
    EASYDDNS_TAG_CONSOLE("**************************************");
    EASYDDNS_PORT.print(request->responseText());      
    request->setDebug(false);
    ddnsIP.fromString(_newIP);
    if (_oldIP != _newIP)
    {
      EASYDDNS_TAG_CONSOLE("_newIP");      
      // Send a callback notification
      if(_ddnsUpdateFunc != nullptr){
        _ddnsUpdateFunc(_oldIP.c_str(), _newIP.c_str());
      }
      // Replace Old IP with new one to detect further changes.
      _oldIP = _newIP;
    }
    else
    {
      EASYDDNS_TAG_CONSOLE("Old IP");
    } 
    EASYDDNS_TAG_CONSOLE("**************************************");     
  }
}

void ESPAsyncEasyDDNS::getIP()
{
  ddnsIP.fromString("0.0.0.0");
  if (_aHttpGetIP.readyState() == 0 || _aHttpGetIP.readyState() == 4)
  {
    bool result = _aHttpGetIP.open("GET", "http://ipv4bot.whatismyipaddress.com/");
    
    if (result)
    {
      // Only send() if open() returns true, or crash
      _aHttpGetIP.send();
    }
    else
    {
      EASYDDNS_TAG_CONSOLE("Can't send bad request");
    }
  }
  else
  {
    EASYDDNS_TAG_CONSOLE("Can't send request");
  }
}

void ESPAsyncEasyDDNS::postIP()
{
  if (_aHttpPostIP.readyState() == 0 || _aHttpPostIP.readyState() == 4)
  {
    String update_url = "";    
    uint8_t auth_service = 0;
    // ######## GENERATE UPDATE URL ######## //
    if (_serverDDNS == "duckdns") {
      update_url = "http://www.duckdns.org/update?domains=" + _domain + "&token=" + _userName + "&ip=" + _newIP + "";
    } else if (_serverDDNS == "noip") {
      auth_service = 1;
      update_url = "http://dynupdate.no-ip.com/nic/update?hostname=" + _domain + "&myip=" + _newIP + "";
    } else if (_serverDDNS == "dyndns") {
      auth_service = 1;
      update_url = "http://members.dyndns.org/v3/update?hostname=" + _domain + "&myip=" + _newIP + "";
    } else if (_serverDDNS == "dynu") {
      update_url = "http://api.dynu.com/nic/update?hostname=" + _domain + "&myip=" + _newIP + "&username=" + _userName + "&password=" + _passWord + "";
    } else if (_serverDDNS == "enom") {
      update_url = "http://dynamic.name-services.com/interface.asp?command=SetDnsHost&HostName=" + _domain + "&Zone=" + _userName + "&DomainPassword=" + _passWord + "&Address=" + _newIP + "";
    } else if (_serverDDNS == "all-inkl") {
      auth_service = 1;
      update_url = "http://dyndns.kasserver.com/?myip=" + _newIP;
    } else if (_serverDDNS == "selfhost.de") {
      auth_service = 1;
      update_url = "http://carol.selfhost.de/nic/update?";
    } else if (_serverDDNS == "dyndns.it") {
      auth_service = 1;
      update_url = "http://update.dyndns.it/nic/update?hostname=" + _domain;
    } else if (_serverDDNS == "strato") {
      auth_service = 1;
      update_url = "http://dyndns.strato.com/nic/update?hostname=" + _domain + "&myip=" + _newIP + "";
    } else if (_serverDDNS == "freemyip") {
      update_url = "http://freemyip.com/update?domain=" + _domain + "&token=" + _userName + "&myip=" + _newIP + "";
    } else if (_serverDDNS == "afraid.org") {
      update_url = "http://sync.afraid.org/u/" + _userName + "/";
    } else {
      EASYDDNS_TAG_CONSOLE("## INPUT CORRECT DDNS SERVICE NAME ##");
      return;
    }     

    EASYDDNS_TAG_CONSOLE("URL: %s", update_url.c_str());
    bool result = _aHttpPostIP.open("GET", update_url.c_str());
    if(auth_service)
    {
      String auth = _userName + ":" + _passWord;
      _base64Authorization = base64::encode(auth);
      // EASYDDNS_TAG_CONSOLE("auth (%s)-(%s)", auth.c_str(), FPSTR(_base64Authorization.c_str()));
      _aHttpPostIP.setReqHeader("Authorization: Basic ", FPSTR(_base64Authorization.c_str()));
    } 
    if (result)
    {
      // Only send() if open() returns true, or crash
      _aHttpPostIP.send();
    }
    else
    {
      EASYDDNS_TAG_CONSOLE("Can't send bad request");
    }
  }
  else
  {
    EASYDDNS_TAG_CONSOLE("Can't send request");
  }
}

void ESPAsyncEasyDDNS::update() 
{
  EASYDDNS_TAG_CONSOLE("Update IP");
  // ######## GET PUBLIC IP ######## //
  getIP();
}
