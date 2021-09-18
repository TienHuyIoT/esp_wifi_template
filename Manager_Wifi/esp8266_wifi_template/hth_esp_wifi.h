#ifndef _HTH_ESP_WIFI_H
#define _HTH_ESP_WIFI_H

#include <Arduino.h>
#include <Ticker.h>
#include <DNSServer.h>
#include "app_config.h"
#include "AsyncEasyDDNS.h"

class hth_esp_sntp
{
private:
    static constexpr char *_ntpServer1 = "pool.ntp.org";
    static constexpr char *_ntpServer2 = "time.nist.gov";
    static constexpr long _gmtOffset_sec = 3600 * 7;
    static constexpr int _daylightOffset_sec = 0;
public:
    hth_esp_sntp(/* args */);
    ~hth_esp_sntp();
    void begin();
};

class hth_esp_wifi
{
private:
#if (defined SNTP_SERVICE_ENABLE) && (SNTP_SERVICE_ENABLE == 1)  
    hth_esp_sntp* _sntp;
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
    WiFiEventHandler accessPointConnectedHandler;
    WiFiEventHandler accessPointDisconnectedHandler;
    WiFiEventHandler accessPointGotIpHandler;
#endif

    static int getRSSIasQuality(int RSSI);
    void registerEventHandler();   // wifi event

public:
    hth_esp_wifi(/* args */);
    ~hth_esp_wifi();

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
    static AsyncEasyDDNSClass* ddnsClient;
#endif

    void begin(bool wifiON = true);
    void end();
    void connect(const char* name, const char* pass);
    void loop();
    int ssidScan(String &json);
};

extern hth_esp_wifi HTH_espWifi;

#endif // _HTH_ESP_WIFI_H