#ifndef __ESP_WIFI_HANDLE_H
#define __ESP_WIFI_HANDLE_H

#include <Arduino.h>
#include <functional> // std::function
#include <DNSServer.h>
#include "THIoT_PFTicker.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPSntp.h"
#include "THIoT_ESPAsyncEasyDDNS.h"
#include "THIoT_ESPEventSignal.h"

#define ESP_RECONNECTION_SECOND_NUM     60 /* 60S */
#define COUNTER_RECONNECT_RESET_NUM     15 /* 15 * 60 = 15'*/
#define INTERVAL_RECONNECT_SECOND_NUM   15

typedef std::function<void(int type)> WiFiLedStatusHandler;
typedef std::function<void(bool evt, IPAddress ip)> WiFiConnectionHandler;
/* OTA state reported to the application (see ESPWifiHandle::enum below) */
typedef std::function<void(int evt)> WiFiOTAHandler;

class ESPWifiHandle
{
private:
#if (defined SNTP_SERVICE_SYSTEM) && (SNTP_SERVICE_SYSTEM == 1)  
    static ESPSntpService _sntp;
#endif
    
#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1) 
    static DNSServer _dnsServer;
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
    static ESPAsyncEasyDDNS* _DDNSService;
#endif

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)
    void onDDNSclient();  // DDNS client service
#endif

    static ticker_function_handle_t _reconnectTicker;
    static boolean _firstConnection;
    static boolean _connectedEvent;
    static uint8_t _counterReconnect;
    static uint8_t _counterReset;
#ifdef ESP8266
    WiFiEventHandler _accessPointConnectedHandler;
    WiFiEventHandler _accessPointDisconnectedHandler;
    WiFiEventHandler _accessPointGotIpHandler;
#endif

    static int getRSSIasQuality(int RSSI);
    void registerEventHandler();   // wifi event
    static boolean _IsConnected;
    static boolean _IsOTA;
    static WiFiLedStatusHandler _ledStatusFunc;
    static WiFiConnectionHandler _connectionFunc;
    static WiFiOTAHandler _otaFunc;
public:
    ESPWifiHandle(/* args */);
    ~ESPWifiHandle();

    void begin(bool wifiON = true);
    void end();
    void connect(const char* name, const char* pass);
    void loop();
    int ssidScan(String &json);
    void OnLedStatus(WiFiLedStatusHandler handler) { _ledStatusFunc = handler; }
    void OnDDNSService(ESPAsyncEasyDDNS* DDNSService) { _DDNSService = DDNSService; }
    void onConnection(WiFiConnectionHandler handler) { _connectionFunc = handler; }
    void onOTA(WiFiOTAHandler handler) { _otaFunc = handler; }
    boolean IsConnected() { return _IsConnected; }
    boolean IsOTA() { return _IsOTA; }
public:
    /* OTA state machine reported through onOTA() */
    enum {
        OTA_STOP = 0,
        OTA_START = 1,
        OTA_PROGRESS = 2,
        OTA_SUCCESS = 3
    };

    void OTAStart();
    void OTAStop();
    void OTASuccess();
};

extern ESPWifiHandle ESPWifi;

#endif // __ESP_WIFI_HANDLE_H