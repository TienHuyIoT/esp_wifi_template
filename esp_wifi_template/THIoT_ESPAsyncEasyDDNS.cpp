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
#include <ArduinoJson.h>
#include "THIoT_ESPAsyncEasyDDNS.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_APPConfig.h"
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_ESPLogTrace.h"

#if (defined ASYNC_EASYDDNS_DEBUG) && (ASYNC_EASYDDNS_DEBUG == 1)
#include "THIoT_PFSerialTrace.h"
#define EASYDDNS_PORT             SERIAL_PORT
#define EASYDDNS_CONSOLE(...)     CONSOLE_LOGI(__VA_ARGS__)
#define EASYDDNS_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[EASYDDNS]", __VA_ARGS__)
#define EASYDDNS_FUN_TAG_CONSOLE(...) CONSOLE_FUNCTION_TAG_LOGI("[EASYDDNS]", __VA_ARGS__)
#else
#define EASYDDNS_CONSOLE(f_, ...)
#define EASYDDNS_TAG_CONSOLE(...)
#define EASYDDNS_FUN_TAG_CONSOLE(...)
#endif
#define DDNS_TAG_LOG(...)         FS_TAG_LOGI("[EASYDDNS]", __VA_ARGS__)

#define DDNS_BUFFER_RESPONSE_SIZE 1024

ESPAsyncEasyDDNS::ESPAsyncEasyDDNS() {
  using namespace std::placeholders;  
  _ddnsIP.fromString("0.0.0.0");
  _serviceDDNS = "";
  _serverDDNS = "";
  _urlGetIp = "";
  _domain = "";
  _userName = "";
  _passWord = "";
  _isStart = false;
  _ddnsUpdateFunc = nullptr;

  _aHttpGetIP.setDebug(false);  
  _aHttpGetIP.onReadyStateChange(std::bind(&ESPAsyncEasyDDNS::getIpCallback, this, _1, _2, _3));
  _aHttpGetIP.onData([](void* arg, asyncHTTPrequest* client, size_t available){
    if (available > DDNS_BUFFER_RESPONSE_SIZE) {
      client->close();
      DDNS_TAG_LOG("_aHttpGetIP is overbuffer %u", DDNS_BUFFER_RESPONSE_SIZE);
    }
  }, this);

  _aHttpPostIP.setDebug(false); 
  _aHttpPostIP.onReadyStateChange(std::bind(&ESPAsyncEasyDDNS::postIpCallback, this, _1, _2, _3));
  _aHttpPostIP.onData([](void* arg, asyncHTTPrequest* client, size_t available){
    if (available > DDNS_BUFFER_RESPONSE_SIZE) {
      client->close();
      DDNS_TAG_LOG("_aHttpPostIP is overbuffer %u", DDNS_BUFFER_RESPONSE_SIZE);
    }
  }, this);
}

// Host record IP
void ESPAsyncEasyDDNS::server(String serverDDNS) {
  _serverDDNS = serverDDNS;
}

// Host record IP
void ESPAsyncEasyDDNS::service(String serviceDDNS) {
  _serviceDDNS = serviceDDNS;
}

// Host get IP
void ESPAsyncEasyDDNS::urlGetIp(String url) {
  _urlGetIp = url;
}

void ESPAsyncEasyDDNS::begin(String ddns_domain,
                             String ddns_username,
                             String ddns_password,
                             int interval,
                             bool enable)
{
  _domain = ddns_domain;
  _userName = ddns_username;
  _passWord = ddns_password;
  _interval = max(interval, 60);
  _isStart = enable;
  if (enable) {
    EASYDDNS_TAG_CONSOLE("Service %s enable", _serviceDDNS);
    _start(_interval);
  } else {
    EASYDDNS_TAG_CONSOLE("Service %s disable", _serviceDDNS);
    ticker_detach(&_ddnsTicker);
  }
}

void ESPAsyncEasyDDNS::_start(uint16_t interval) {
  ticker_attach(&_ddnsTicker, interval, [](void *arg) {
    ESPAsyncEasyDDNS *handler = (ESPAsyncEasyDDNS*)arg;
      handler->_update();
  }, this);
}

void ESPAsyncEasyDDNS::getIpCallback(void* optParm, asyncHTTPrequest* request, int readyState) {
  EASYDDNS_TAG_CONSOLE("%s, readyState = %u", _urlGetIp.c_str(), readyState);
  if (4 == readyState) {
    int httpCode = request->responseHTTPcode();
    EASYDDNS_TAG_CONSOLE("httpCode = %d", httpCode);
    if (200 != httpCode) {
      EASYDDNS_TAG_CONSOLE("Close");
      _aHttpGetIP.close();
      delay(10);
    }
    else {
      _newIP = request->responseText();
      EASYDDNS_TAG_CONSOLE("**************************************");
      EASYDDNS_TAG_CONSOLE("getIP: %s", _newIP.c_str());
      EASYDDNS_TAG_CONSOLE("**************************************");
      IPAddress ipV4;
      bool status = ipV4.fromString(_newIP);
      if (!status) {
        DDNS_TAG_LOG("Error newIP: %s", _newIP.c_str());
      } else {
        if (_oldIP != _newIP) {
          _oldIP = _newIP;
          _updateStatus = true;
          EASYDDNS_TAG_CONSOLE("IP is new");
          DDNS_TAG_LOG("newIP: %s", _newIP.c_str());
        }

        if (_serviceDDNS != "private") {
          postIP2DynDNS();
        } else {
          postIP2Customer();
        }
      }
    }
  }
}

void ESPAsyncEasyDDNS::postIpCallback(void* optParm, asyncHTTPrequest* request, int readyState) {
  EASYDDNS_TAG_CONSOLE("%s, readyState = %u", _serviceDDNS.c_str(), readyState);
  if (4 == readyState) {
    int httpCode = request->responseHTTPcode();
    EASYDDNS_TAG_CONSOLE("httpCode = %d", httpCode);
    if (200 != httpCode) {
      EASYDDNS_TAG_CONSOLE("Close");
      _aHttpPostIP.close();
      delay(10);
    }
    else {
      EASYDDNS_TAG_CONSOLE("**************************************");
      EASYDDNS_TAG_CONSOLE("Data: %s", request->responseText().c_str());
      _ddnsIP.fromString(_newIP);
      EASYDDNS_TAG_CONSOLE("update IP succeed");
      if (_updateStatus) {
        _updateStatus = false;
        DDNS_TAG_LOG("update IP %s succeed", _newIP.c_str());  
        // Send a callback notification
        if(_ddnsUpdateFunc != nullptr) {
          _ddnsUpdateFunc(_oldIP.c_str(), _newIP.c_str());
        }
        updateTrigger(300); // 5' update one time.
      }
      EASYDDNS_TAG_CONSOLE("**************************************");
    }  
  }
}

void ESPAsyncEasyDDNS::getPublicIP() {
  int readyState = _aHttpGetIP.readyState();
  if (0 != readyState && 4 != readyState) {
    EASYDDNS_TAG_CONSOLE("[GetIP] close last connection");
#if (HTTP_OPTION_CLOSE_CLIENT == 1)
    _aHttpGetIP.close();
#else
    _aHttpGetIP.abort();
#endif
    delay(10); // wait for closing client
  }

  EASYDDNS_TAG_CONSOLE("Get ip from %s", _urlGetIp.c_str());
  bool result = _aHttpGetIP.open("GET", _urlGetIp.c_str());
  if (result) {
    // Only send() if open() returns true, or crash
    _aHttpGetIP.send();
  }
  else {
    EASYDDNS_TAG_CONSOLE("Can't send bad request");
    updateTrigger(60); /* Trigger update again by 60s */
  }
}

void ESPAsyncEasyDDNS::postIP2Customer() {
  int readyState = _aHttpPostIP.readyState();
  if (0 != readyState && 4 != readyState) {
    EASYDDNS_FUN_TAG_CONSOLE("close last connection");
#if (HTTP_OPTION_CLOSE_CLIENT == 1)
    _aHttpPostIP.close();
#else
    _aHttpPostIP.abort();
#endif
    delay(10); // wait for closing client
  }
  // Create data
  const size_t capacity = JSON_OBJECT_SIZE(25);
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject& root = jsonBuffer.createObject();
  root["hostname"] = _domain;
  root["myip"] = _newIP;
  if (_priDataHandle) {
    _priDataHandle(_priDataHandleArg, root);
  }

  String body;
  if (!body.reserve(root.measurePrettyLength() + 1)) {
    return;
  }
  root.prettyPrintTo(body);
  EASYDDNS_TAG_CONSOLE("Body: %s", body.c_str());

  String update_url = _serverDDNS + "/update?hostname=" + _domain + "&myip=" + _newIP + "";
  EASYDDNS_TAG_CONSOLE("Post IP  to URL: %s", update_url.c_str());
  _aHttpPostIP.setTimeout(5);
  bool result = _aHttpPostIP.open("POST", update_url.c_str());
  if(result) {
    String auth = _userName + ":" + _passWord;
    _base64Authorization = base64::encode(auth);
    _aHttpPostIP.setReqHeader("Authorization: Basic ", FPSTR(_base64Authorization.c_str()));
    _aHttpPostIP.setReqHeader("Content-Type" , "application/json");
    _aHttpPostIP.send(body.c_str());
  }
  else {
    EASYDDNS_TAG_CONSOLE("Can't send bad request");
    updateTrigger(30); /* Trigger update again by 30s */
  }

}

void ESPAsyncEasyDDNS::postIP2DynDNS() {
  int readyState = _aHttpPostIP.readyState();
  if (0 != readyState && 4 != readyState) {
    EASYDDNS_TAG_CONSOLE("[PostIP] close last connection");
#if (HTTP_OPTION_CLOSE_CLIENT == 1)
    _aHttpPostIP.close();
#else
    _aHttpPostIP.abort();
#endif
    delay(10); // wait for closing client
  }

  String update_url = "";    
  uint8_t auth_service = 0;
  // ######## GENERATE UPDATE URL ######## //
  if (_serviceDDNS == "duckdns") {
    update_url = "http://www.duckdns.org/update?domains=" + _domain + "&token=" + _userName + "&ip=" + _newIP + "";
  } else if (_serviceDDNS == "private") {
    auth_service = 1;
    update_url = _serverDDNS + "/update?hostname=" + _domain + "&myip=" + _newIP + "";
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

  EASYDDNS_TAG_CONSOLE("Post IP  to URL: %s", update_url.c_str());
  bool result = _aHttpPostIP.open("GET", update_url.c_str());
  if (result) {
    if(auth_service) {
      String auth = _userName + ":" + _passWord;
      _base64Authorization = base64::encode(auth);
      // EASYDDNS_TAG_CONSOLE("auth (%s)-(%s)", auth.c_str(), FPSTR(_base64Authorization.c_str()));
      _aHttpPostIP.setReqHeader("Authorization: Basic ", FPSTR(_base64Authorization.c_str()));
    } 
    // Only send() if open() returns true, or crash
    _aHttpPostIP.setTimeout(5);
    _aHttpPostIP.send();
  }
  else {
    EASYDDNS_TAG_CONSOLE("Can't send bad request");
    updateTrigger(60); /* Trigger update again by 60s */
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
  if(ESPWifi.IsConnected() || Ethernet.IsConnected()) {
    EASYDDNS_TAG_CONSOLE("Update IP");
    // ######## GET PUBLIC IP ######## //
    getPublicIP();
  }
}

ESPAsyncEasyDDNS EasyDDNS;
ESPAsyncEasyDDNS MonTechDDNS;