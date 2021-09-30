#ifndef __ESP_WIFI_HANDLE_H
#define __ESP_WIFI_HANDLE_H

#include <Arduino.h>
#include <functional> // std::function
#include <Ticker.h>
#include <DNSServer.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPAsyncEasyDDNS.h"
#include "THIoT_ESPEventSignal.h"

typedef std::function<void(int type)> WiFiLedStatusHandler;

class ESPSntpService
{
private:
    static String _server1;
    static String _server2;

public:
    ESPSntpService(/* args */);
    ~ESPSntpService();
    void begin();
};

class ESPWifiHandle
{
private:
#if (defined SNTP_SERVICE_SYSTEM) && (SNTP_SERVICE_SYSTEM == 1)  
    ESPSntpService* _sntp;
#endif
    
#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1) 
    DNSServer* _dnsServer;
    void onDNSServer();   // DNS server AP mode
#endif

#if (defined MDNS_SERVICE_ENABLE) && (MDNS_SERVICE_ENABLE == 1)  
    void onMDNSService(); // local server name for MAC OS
#endif

#if (defined NBNS_SERVICE_ENABLE) && (NBNS_SERVICE_ENABLE == 1)
    void onNBNSService(); // local server name for WIN OS
#endif

#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)
    static uint32_t _otaArduinoPercent;
    void onArduinoOTA();  // Arduino OTA
#endif

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
    Ticker _ddnsTicker;
    void onDDNSclient();  // DDNS client service
#endif

    static Ticker _reconnetTicker;
#ifdef ESP8266
    WiFiEventHandler _accessPointConnectedHandler;
    WiFiEventHandler _accessPointDisconnectedHandler;
    WiFiEventHandler _accessPointGotIpHandler;
#endif

    static int getRSSIasQuality(int RSSI);
    void registerEventHandler();   // wifi event
    static bool _wifiConnected;
    static WiFiLedStatusHandler _ledStatusFunc;
public:
    ESPWifiHandle(/* args */);
    ~ESPWifiHandle();

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
    static ESPAsyncEasyDDNS* ddnsClient;
#endif

    void begin(bool wifiON = true);
    void end();
    void connect(const char* name, const char* pass);
    void loop();
    int ssidScan(String &json);
    void onLedStatus(WiFiLedStatusHandler handler)
    {
        _ledStatusFunc = handler;
    }
};

extern ESPWifiHandle ESPWifi;

#endif // __ESP_WIFI_HANDLE_H