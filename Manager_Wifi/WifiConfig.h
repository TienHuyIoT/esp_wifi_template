#ifndef	_WIFICONFIG_H
#define _WIFICONFIG_H

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <stdint.h>
#include <TimeOutEvent.h>
#include <IOBlink.h>

#ifdef ESP8266
#define WIFI_PORT Serial
#elif defined(ESP32)
#define WIFI_PORT Serial
#endif
#define WIFI_DEBUG(fmt, ...) WIFI_PORT.printf("\r\n>Wifi Config< " fmt, ##__VA_ARGS__)

class WifiConfigClass
{
  private:
    TimeOutEvent WifiSTATo;
    TimeOutEvent WifiSTADisTo;
    wl_status_t wlStatus;
    IOBlink *LedStatus;
    IOBlink *BuzzStatus;
    bool LedPresent;
    bool BuzzPresent;
  public:
    WifiConfigClass();
    bool WifiConnect(void);
    void WifiConnectStatusProc(void);
    void WifiInit(void);
    void WifiLedStatus(IOBlink *); 
    void WifiBuzzStatus(IOBlink *); 
    wl_status_t WifiStatus(); 
};

extern WifiConfigClass WifiConfig;
#endif




