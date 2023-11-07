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
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_ESPLogTrace.h"

#if (defined ASYNC_EASYDDNS_DEBUG) && (ASYNC_EASYDDNS_DEBUG == 1)
#include "THIoT_PFSerialTrace.h"
#define EASYDDNS_PORT SERIAL_PORT
#define EASYDDNS_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#define EASYDDNS_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[EASYDDNS]", __VA_ARGS__)
#else
#define EASYDDNS_CONSOLE(f_, ...)
#define EASYDDNS_TAG_CONSOLE(...)
#endif
#define DDNS_TAG_LOG(...) FS_TAG_LOGI("[EASYDDNS]", __VA_ARGS__)

ESPAsyncEasyDDNS::ESPAsyncEasyDDNS() {
  using namespace std::placeholders;  
  _ddnsIP.fromString("0.0.0.0");
  _serviceDDNS = "";
  _urlGetIp = "";
  _domain = "";
  _userName = "";
  _passWord = "";
  _isStart = false;
  _ddnsUpdateFunc = nullptr;
  _aHttpGetIP.setDebug(false);  
  _aHttpPostIP.setDebug(false); 
  _aHttpGetIP.onReadyStateChange(std::bind(&ESPAsyncEasyDDNS::getIpCallback, this, _1, _2, _3));
  _aHttpPostIP.onReadyStateChange(std::bind(&ESPAsyncEasyDDNS::postIpCallback, this, _1, _2, _3));
}

void ESPAsyncEasyDDNS::service(String serviceDDNS) {
  _serviceDDNS = serviceDDNS;
}

void ESPAsyncEasyDDNS::urlGetIp(String url) {
  _urlGetIp = url;
}

void ESPAsyncEasyDDNS::begin(String ddns_domain, String ddns_username, String ddns_password, int interval) {
  _domain = ddns_domain;
  _userName = ddns_username;
  _passWord = ddns_password;
  _interval = max(interval, 30);
  _isStart = true;
  _start(_interval);
}

void ESPAsyncEasyDDNS::_start(uint16_t interval) {
  ticker_attach(&_ddnsTicker, interval, [](void *arg) {
    ESPAsyncEasyDDNS *handler = (ESPAsyncEasyDDNS*)arg;
      handler->_update();
  }, this);
}

void ESPAsyncEasyDDNS::getIpCallback(void* optParm, asyncHTTPrequest* request, int readyState) {
  if (readyState == 4 && request->responseHTTPcode() == 200) {
    _newIP = request->responseText();
    EASYDDNS_TAG_CONSOLE("**************************************");
    EASYDDNS_TAG_CONSOLE("getIP: %s", _newIP.c_str());
    EASYDDNS_TAG_CONSOLE("**************************************");    
    request->setDebug(false);
    if (_oldIP != _newIP) {
      _oldIP = _newIP;
      _updateStatus = true;
      EASYDDNS_TAG_CONSOLE("IP is new");
      DDNS_TAG_LOG("newIP: %s", _newIP.c_str());
    }
    postIP();    
  }
}

void ESPAsyncEasyDDNS::postIpCallback(void* optParm, asyncHTTPrequest* request, int readyState) {
  if (readyState == 4 && request->responseHTTPcode() == 200) {
    EASYDDNS_TAG_CONSOLE("**************************************");
    EASYDDNS_PORT.print(request->responseText());      
    request->setDebug(false);
    _ddnsIP.fromString(_newIP);
    EASYDDNS_TAG_CONSOLE("update IP succeed");
    if (_updateStatus) {
      _updateStatus = false;
      DDNS_TAG_LOG("update IP %s succeed", _newIP.c_str());  
      // Send a callback notification
      if(_ddnsUpdateFunc != nullptr) {
        _ddnsUpdateFunc(_oldIP.c_str(), _newIP.c_str());
      }
    }
    EASYDDNS_TAG_CONSOLE("**************************************");     
  }
}

void ESPAsyncEasyDDNS::getPublicIP() {
  int readyState = _aHttpGetIP.readyState();
  if (0 != readyState && 4 != readyState) {
    EASYDDNS_TAG_CONSOLE("[GetIP] abort last connection");
    _aHttpGetIP.abort();
  }

  _ddnsIP.fromString("0.0.0.0");
  bool result = _aHttpGetIP.open("GET", _urlGetIp.c_str());
  if (result) {
    // Only send() if open() returns true, or crash
    _aHttpGetIP.send();
  }
  else {
    EASYDDNS_TAG_CONSOLE("Can't send bad request");
    updateTrigger(30); /* Trigger update again by 30s */
  }
}

void ESPAsyncEasyDDNS::postIP() {
  int readyState = _aHttpPostIP.readyState();
  if (0 != readyState && 4 != readyState) {
    EASYDDNS_TAG_CONSOLE("[PostIP] abort last connection");
    _aHttpPostIP.abort();
  }

  String update_url = "";    
  uint8_t auth_service = 0;
  // ######## GENERATE UPDATE URL ######## //
  if (_serviceDDNS == "duckdns") {
    update_url = "http://www.duckdns.org/update?domains=" + _domain + "&token=" + _userName + "&ip=" + _newIP + "";
  } else if (_serviceDDNS == "noip") {
    auth_service = 1;
    update_url = "http://dynupdate.no-ip.com/nic/update?hostname=" + _domain + "&myip=" + _newIP + "";
  } else if (_serviceDDNS == "dyndns") {
    auth_service = 1;
    update_url = "http://members.dyndns.org/v3/update?hostname=" + _domain + "&myip=" + _newIP + "";
  } else if (_serviceDDNS == "dynu") {
    update_url = "http://api.dynu.com/nic/update?hostname=" + _domain + "&myip=" + _newIP + "&username=" + _userName + "&password=" + _passWord + "";
  } else if (_serviceDDNS == "enom") {
    update_url = "http://dynamic.name-services.com/interface.asp?command=SetDnsHost&HostName=" + _domain + "&Zone=" + _userName + "&DomainPassword=" + _passWord + "&Address=" + _newIP + "";
  } else if (_serviceDDNS == "all-inkl") {
    auth_service = 1;
    update_url = "http://dyndns.kasserver.com/?myip=" + _newIP;
  } else if (_serviceDDNS == "selfhost.de") {
    auth_service = 1;
    update_url = "http://carol.selfhost.de/nic/update?";
  } else if (_serviceDDNS == "dyndns.it") {
    auth_service = 1;
    update_url = "http://update.dyndns.it/nic/update?hostname=" + _domain;
  } else if (_serviceDDNS == "strato") {
    auth_service = 1;
    update_url = "http://dyndns.strato.com/nic/update?hostname=" + _domain + "&myip=" + _newIP + "";
  } else if (_serviceDDNS == "freemyip") {
    update_url = "http://freemyip.com/update?domain=" + _domain + "&token=" + _userName + "&myip=" + _newIP + "";
  } else if (_serviceDDNS == "afraid.org") {
    update_url = "http://sync.afraid.org/u/" + _userName + "/";
  } else {
    EASYDDNS_TAG_CONSOLE("## INPUT CORRECT DDNS SERVICE NAME ##");
    return;
  }     

  EASYDDNS_TAG_CONSOLE("URL: %s", update_url.c_str());
  bool result = _aHttpPostIP.open("GET", update_url.c_str());
  if(auth_service) {
    String auth = _userName + ":" + _passWord;
    _base64Authorization = base64::encode(auth);
    // EASYDDNS_TAG_CONSOLE("auth (%s)-(%s)", auth.c_str(), FPSTR(_base64Authorization.c_str()));
    _aHttpPostIP.setReqHeader("Authorization: Basic ", FPSTR(_base64Authorization.c_str()));
  } 
  if (result) {
    // Only send() if open() returns true, or crash
    _aHttpPostIP.send();
  }
  else {
    EASYDDNS_TAG_CONSOLE("Can't send bad request");
    updateTrigger(30); /* Trigger update again by 30s */
  }
}

/**
 * @brief trigger update ddns by timeout 
 * without waiting for an interval trigger
*/
void ESPAsyncEasyDDNS::updateTrigger(uint16_t timeout) {
  if (_isStart == false) {
    return;
  }
  ticker_attach_trigger(&_ddnsTicker, timeout);
}

void ESPAsyncEasyDDNS::_update() {
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
  if(ESPWifi.IsConnected() || Ethernet.IsConnected())
#else
  if(ESPWifi.IsConnected())
#endif
  {
    EASYDDNS_TAG_CONSOLE("Update IP");
    // ######## GET PUBLIC IP ######## //
    getPublicIP();
  }
}

ESPAsyncEasyDDNS EasyDDNS;