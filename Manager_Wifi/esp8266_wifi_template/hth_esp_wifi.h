#ifndef _HTH_ESP_WIFI_H
#define _HTH_ESP_WIFI_H

#include <Arduino.h>
#include <Ticker.h>
#include <DNSServer.h>
#include "hth_esp_config.h"
#include "hth_AsyncEasyDDNS.h"

class ESPSntpService
{
private:
    static constexpr char *_ntpServer1 = "pool.ntp.org";
    static constexpr char *_ntpServer2 = "time.nist.gov";
    static constexpr long _gmtOffset_sec = 3600 * 7;
    static constexpr int _daylightOffset_sec = 0;
public:
    ESPSntpService(/* args */);
    ~ESPSntpService();
    void begin();
};

class ESPWifiHandler
{
private:
#if (defined SNTP_SERVICE_ENABLE) && (SNTP_SERVICE_ENABLE == 1)  
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

public:
    ESPWifiHandler(/* args */);
    ~ESPWifiHandler();

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
    static hth_AsyncEasyDDNSClass* ddnsClient;
#endif

    void begin(bool wifiON = true);
    void end();
    void connect(const char* name, const char* pass);
    void loop();
    int ssidScan(String &json);
};

extern ESPWifiHandler ESPWifi;

#endif // _HTH_ESP_WIFI_H