#ifdef ESP32
#include <esp_idf_version.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiType.h>
#include <time.h>
#if ESP_IDF_VERSION_MAJOR >= 4
#include <esp_sntp.h>
#else
#include <lwip/apps/sntp.h>
#endif
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <user_interface.h>
#include <TZ.h>
#include <time.h>      // time() ctime()
#include <sys/time.h>  // struct timeval
#include <coredecls.h> // settimeofday_cb()
#include <sntp.h>      // sntp_servermode_dhcp()

#define MYTZ TZ_Asia_Ho_Chi_Minh

// OPTIONAL: change SNTP startup delay
// a weak function is already defined and returns 0 (RFC violation)
// it can be redefined:
#if (0)
uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000 ()
{
   //info_sntp_startup_delay_MS_rfc_not_less_than_60000_has_been_called = true;
   return 60000; // 60s (or lwIP's original default: (random() % 5000))
}
#endif

// OPTIONAL: change SNTP update delay
// a weak function is already defined and returns 1 hour
// it can be redefined:
#if (0)
uint32_t sntp_update_delay_MS_rfc_not_less_than_15000 ()
{
  //  info_sntp_update_delay_MS_rfc_not_less_than_15000_has_been_called = true;
   return 15000; // 15s
}
#endif
#endif

#include "THIoT_ESPConfig.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_ESPAsyncEasyNTP.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_PFLedStatus.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_ESPSoftReset.h"

#define EASY_SNTP_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[EASY_NTP]", __VA_ARGS__)

#define ESP_WIFI_PORT SERIAL_PORT
#define ESP_WIFI_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#define ESP_WIFI_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[WIFI]", __VA_ARGS__)
#define ESP_ETH_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[ETH]", __VA_ARGS__)

#define ESP_WIFI_TAG_LOG(...) FS_TAG_LOGI("[WIFI]", __VA_ARGS__)
#define ESP_ETH_TAG_LOG(...) FS_TAG_LOGI("[ETH]", __VA_ARGS__)

/* ESPWifiHandle class --------------------------------------------------------
 -----------------------------------------------------------------------------
 ---------------------------------------------------------------------------*/
#if (defined SNTP_SERVICE_SYSTEM) && (SNTP_SERVICE_SYSTEM == 1)  
ESPSntpService ESPWifiHandle::_sntp = ESPSntpService();
#endif
#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1) 
DNSServer ESPWifiHandle::_dnsServer = DNSServer();
#endif
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
ESPAsyncEasyDDNS* ESPWifiHandle::_DDNSService = nullptr;
#endif

ticker_function_handle_t ESPWifiHandle::_reconnectTicker = {0};
boolean ESPWifiHandle::_IsConnected = false;
boolean ESPWifiHandle::_firstConnection = false;
uint8_t ESPWifiHandle::_counterReconnect = 0;
uint8_t ESPWifiHandle::_counterReset = 0;
boolean ESPWifiHandle::_IsOTA = false;
WiFiLedStatusHandler ESPWifiHandle::_ledStatusFunc = nullptr;
WiFiConnectionHandler ESPWifiHandle::_connectionFunc = nullptr;
WiFiOTAHandler ESPWifiHandle::_otaFunc = nullptr;

ESPWifiHandle::ESPWifiHandle(/* args */) {}
ESPWifiHandle::~ESPWifiHandle() {}

void ESPWifiHandle::registerEventHandler() {
  ESP_WIFI_TAG_CONSOLE("registerEventHandler");
#ifdef ESP32
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (!_IsConnected) {
      _IsConnected = true;
      
      ticker_detach(&_reconnectTicker);
      ESP_WIFI_TAG_CONSOLE("[EVENT] got IP address: %s", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
      ESP_WIFI_TAG_LOG("[EVENT] got IP address: %s", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
      if (_ledStatusFunc) {
        _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_WIFI_GOT_IP);
      }
      if (_connectionFunc) {
        _connectionFunc(true, IPAddress(info.got_ip.ip_info.gw.addr));
      }
#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
#if (TIME_ZONE_TYPE_CFG == 1)
      EASY_NTP.begin(ESPConfig.gmtOffsetSNTP(), ESPConfig.daylightOffsetSNTP(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
#else
      EASY_NTP.begin(ESPConfig.TzTime().c_str(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
#endif
#endif
      /* Smart config enable */
      if(ESPConfig.smartCfgSTA()) {
        if(ESPConfig.ssidSTA() != WiFi.SSID()
        || ESPConfig.passSTA() != WiFi.psk())
        {
          ESPConfig.ssidSTASet(WiFi.SSID());
          ESPConfig.passSTASet(WiFi.psk());
          ESPConfig.save();
        }
      }
      /* Run DDNS service */
      _DDNSService->updateTrigger();
    }
  }
  ,WiFiEvent_t::m_ESP32_EVENT_STA_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
#if ESP_IDF_VERSION_MAJOR >= 4
    std::string ssid((const char*)info.wifi_sta_connected.ssid, info.wifi_sta_connected.ssid_len);
#else
    std::string ssid((const char*)info.connected.ssid, info.connected.ssid_len);
#endif
    ESP_WIFI_TAG_CONSOLE("[EVENT] connected to %s", ssid.c_str());
    ESP_WIFI_TAG_LOG("[EVENT] connected to %s", ssid.c_str());
  }
  ,WiFiEvent_t::m_ESP32_EVENT_STA_CONNECTED);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    int reason;
#if ESP_IDF_VERSION_MAJOR >= 4
    reason = info.wifi_sta_disconnected.reason;
#else
    reason = info.disconnected.reason;
#endif
    if (_IsConnected || !_firstConnection) {
      _IsConnected = false;
      _firstConnection = true;
      
      ESP_WIFI_TAG_CONSOLE("[EVENT] lost connection. Reason: %u", reason);
      ESP_WIFI_TAG_LOG("[EVENT] lost connection. Reason: %u", reason);
      if (_ledStatusFunc) {
        _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_WIFI_DISCONNECTED);
      }
      if (_connectionFunc) {
        _connectionFunc(false, IPAddress(0U));
      }
#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
      EASY_NTP.end();
#endif

      /* Note: running setAutoReconnect(true) when module is already disconnected 
      will not make it reconnect to the access point. Instead reconnect() 
      should be used. 
        We don't need call WiFi.reconnect() once loss connection, it is self reconnect after loss connection.
      Reconnecting will be destroy, if executing scan network.
      */
      _counterReconnect = ESP_RECONNECTION_SECOND_NUM;
      _counterReset = 0;
      ticker_attach(&_reconnectTicker, INTERVAL_RECONNECT_SECOND_NUM, [](void *arg) {
          WiFiClass *handler = (WiFiClass*)arg;
          _counterReconnect += INTERVAL_RECONNECT_SECOND_NUM;
          if (_counterReconnect >= ESP_RECONNECTION_SECOND_NUM) {
            _counterReconnect = 0;

            if (++_counterReset > COUNTER_RECONNECT_RESET_NUM) {
              _counterReset = 0;
#if (NETWORK_CONNECTION_TIMEOUT_RESET == 1)
              SOFTReset.enable(500, ESPSoftReset::WIFI_RECONNECT_TYPE);
#endif
            }
            else {
              ESP_WIFI_TAG_CONSOLE("Reconnect");
              handler->reconnect();
            }
          }
      }, &WiFi);
    }
  },
  WiFiEvent_t::m_ESP32_EVENT_STA_DISCONNECTED);

  /**===================================================================
   * =========================BEGIN ETH EVENT===========================
   * ===================================================================
  */
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    ESP_ETH_TAG_CONSOLE("[EVENT] started");
    ESP_ETH_TAG_LOG("[EVENT] started");
    String ChipID = String((uint32_t)(ESP.getEfuseMac() >> 16), HEX);
    ChipID.toUpperCase();
    String hostName = "ETH_" + ESPConfig.hostNameSTA() + "_" + ChipID;
    ETH.setHostname(hostName.c_str());
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_START);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_IsConnected) {
      _IsConnected = false;
      
      Ethernet.disconnectEvt();
      ESP_ETH_TAG_CONSOLE("[EVENT] stopped");
      ESP_ETH_TAG_LOG("[EVENT] stopped");
      if (_connectionFunc) {
        _connectionFunc(false, IPAddress(0U));
      }
    }
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_STOP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (!_IsConnected) {
      _IsConnected = true;
      
      ESP_ETH_TAG_CONSOLE("[EVENT] got IP address: %s", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
      ESP_ETH_TAG_LOG("[EVENT] got IP address: %s", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
      Ethernet.connectedEvt();
      if (_connectionFunc) {
        _connectionFunc(true, IPAddress(info.got_ip.ip_info.gw.addr));
      }
      /* Run DDNS service */
      _DDNSService->updateTrigger();
    }
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    ESP_ETH_TAG_CONSOLE("[EVENT] connected");
    ESP_ETH_TAG_LOG("[EVENT] connected");
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_CONNECTED);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_IsConnected || !_firstConnection) {
      _IsConnected = false;
      _firstConnection = true;
      
      Ethernet.disconnectEvt();
      if (_connectionFunc) {
        _connectionFunc(false, IPAddress(0U));
      }
    }
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_DISCONNECTED);
#endif
  /**===================================================================
   * =========================END ETH EVENT=============================
   * ===================================================================
  */
#elif defined(ESP8266)
  // To register event, must be declare _accessPointGotIpHandler
  _accessPointGotIpHandler = WiFi.onStationModeGotIP(
    [](const WiFiEventStationModeGotIP& evt) {
      if (!_IsConnected) {
        _IsConnected = true;
        
        ticker_detach(&_reconnectTicker);
        ESP_WIFI_TAG_CONSOLE("[EVENT] got IP address: %s", evt.ip.toString().c_str());
        ESP_WIFI_TAG_LOG("[EVENT] got IP address: %s", evt.ip.toString().c_str());
        if (_ledStatusFunc) {
          _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_WIFI_GOT_IP);
        }
        if (_connectionFunc) {
          _connectionFunc(true, evt.gw);
        }
  #if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
  #if (TIME_ZONE_TYPE_CFG == 1)
        EASY_NTP.begin(ESPConfig.gmtOffsetSNTP(), ESPConfig.daylightOffsetSNTP(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
  #else
        EASY_NTP.begin(ESPConfig.TzTime().c_str(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
  #endif
  #endif
        /* Smart config enable */
        if(ESPConfig.smartCfgSTA())
        {
          if(ESPConfig.ssidSTA() != WiFi.SSID()
          || ESPConfig.passSTA() != WiFi.psk())
          {
            ESPConfig.ssidSTASet(WiFi.SSID());
            ESPConfig.passSTASet(WiFi.psk());
            ESPConfig.save();
          }
        }

        /* Run DDNS service */
        _DDNSService->updateTrigger();
      }
    }
  );

  // To register event, must be declare _accessPointConnectedHandler
  _accessPointConnectedHandler = WiFi.onStationModeConnected(
    [](const WiFiEventStationModeConnected& evt) {
      ESP_WIFI_TAG_CONSOLE("[EVENT] connected to %s", evt.ssid.c_str());
      ESP_WIFI_TAG_LOG("[EVENT] connected to %s", evt.ssid.c_str());
    }
  );

  // To register event, must be declare _accessPointDisconnectedHandler
  _accessPointDisconnectedHandler = WiFi.onStationModeDisconnected(
    [](const WiFiEventStationModeDisconnected& evt) {
      if (_IsConnected) {
        _IsConnected = false;
        
        ESP_WIFI_TAG_CONSOLE("[EVENT] lost connection to %s. Reason: %u",
                             evt.ssid.c_str(), evt.reason);
        ESP_WIFI_TAG_LOG("[EVENT] lost connection to %s. Reason: %u",
                             evt.ssid.c_str(), evt.reason);
        if (_ledStatusFunc) {
          _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_WIFI_DISCONNECTED);
        }
        if (_connectionFunc) {
          _connectionFunc(false, IPAddress(0U));
        }
#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
        EASY_NTP.end();
#endif
      }
      /* Note: running setAutoReconnect(true) when module is already disconnected 
      will not make it reconnect to the access point. Instead reconnect() 
      should be used. 
        We don't need call WiFi.reconnect() once loss connection, it is self reconnect after loss connection.
      Reconnecting will be destroy, if executing scan network.
      */
      _counterReconnect = ESP_RECONNECTION_SECOND_NUM;
      _counterReset = 0;
      ticker_attach(&_reconnectTicker, INTERVAL_RECONNECT_SECOND_NUM, [](void *arg) {
          WiFiClass *handler = (WiFiClass*)arg;
          _counterReconnect += INTERVAL_RECONNECT_SECOND_NUM;
          if (_counterReconnect >= ESP_RECONNECTION_SECOND_NUM) {
            _counterReconnect = 0;

            if (++_counterReset > COUNTER_RECONNECT_RESET_NUM) {
              _counterReset = 0;
#if (NETWORK_CONNECTION_TIMEOUT_RESET == 1)
              SOFTReset.enable(500, ESPSoftReset::WIFI_RECONNECT_TYPE);
#endif
            }
            else {
              ESP_WIFI_TAG_CONSOLE("Reconnect");
              handler->reconnect();
            }
          }
      }, &WiFi);
    }
  );
#endif // ESP32
}

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
void ESPWifiHandle::onDDNSclient() {
  if (ESPConfig.disableDDNS()) {
      ESP_WIFI_TAG_CONSOLE("[DDNS] client disable");
      return;
  }
    /*
    List of supported DDNS providers:
    - "duckdns"
    - "noip"
    - "dyndns"
    - "dynu"
    - "enom"
    - "all-inkl"
    - "selfhost.de"
    - "dyndns.it"
    - "strato"
    - "freemyip"
    - "afraid.org"
  */
  _DDNSService->service(ESPConfig.serviceDDNS());
  _DDNSService->urlGetIp(ESPConfig.ipUrlDDNS());

  // Get Notified when your IP changes
  _DDNSService->onUpdateIP([&](const char* oldIP, const char* newIP) {
    ESP_WIFI_TAG_CONSOLE("[DDNS] AsyncEasyDDNS - IP Change Detected: %s", newIP);
  });

  /*
    For DDNS Providers where you get a token:
      Use this: _DDNSService->begin("domain", "token");
    
    For DDNS Providers where you get username and password: ( Leave the password field empty "" if not required )
      Use this: _DDNSService->begin("domain", "username", "password");
  */
  _DDNSService->begin(ESPConfig.domainDDNS(), ESPConfig.userDDNS(), ESPConfig.passDDNS(), ESPConfig.syncTimeDDNS());
}
#endif

/* Should be called on wifi event gotIP */
#if (defined MDNS_SERVICE_ENABLE) && (MDNS_SERVICE_ENABLE == 1)
void ESPWifiHandle::onMDNSService()
{
  // Set up mDNS responder:
  // - first argument is the domain name, in this example
  //   the fully-qualified domain name is "esp32.local"
  // - second argument is the IP address to advertise
  //   we send our IP address on the WiFi network
  if (!MDNS.begin(ESPConfig.hostNameSTA().c_str()))
  {
    ESP_WIFI_TAG_CONSOLE("Error setting up MDNS responder!");
  }
  else
  {
    ESP_WIFI_TAG_CONSOLE("mDNS responder started");
  }

  /** alway 80, because we have redirect port 80 to server port 
   * This function must be run after MDNS.begin()
  */
  MDNS.addService("http", "tcp", 80);
}
#endif

#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1)
void ESPWifiHandle::onDNSServer()
{
  constexpr uint16_t DNS_SERVER_PORT = 53;
  _dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  _dnsServer.start(DNS_SERVER_PORT, ESPConfig.dnsNameAP(), WiFi.softAPIP());  
}
#endif

/** Should be called on wifi event gotIP*/
#if (defined NBNS_SERVICE_ENABLE) && (NBNS_SERVICE_ENABLE == 1)
void ESPWifiHandle::onNBNSService()
{
  NBNS.begin(ESPConfig.hostNameSTA().c_str());
}
#endif

/** Should be called after init wifi */
#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)
uint32_t ESPWifiHandle::_otaArduinoPercent = 0;
void ESPWifiHandle::onArduinoOTA()
{
  ArduinoOTA.onStart(
      [&]()
      {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
          type = "application";
        }
        else // U_SPIFFS
        {
          type = "filesystem";
          NAND_FS_SYSTEM.end();
        }
        OTAStart();
        ticker_detach(&_reconnectTicker); // disable wifi auto reconnect access point

#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
        EASY_NTP.end();
#endif

        /** NOTE: if updating SPIFFS this would be the place
         *  to unmount SPIFFS using SPIFFS.end() */
        ESP_WIFI_TAG_CONSOLE("[OTA] Start updating %s", type.c_str());
        ESP_WIFI_TAG_LOG("[OTA] Start updating %s", type.c_str());
      });

  ArduinoOTA.onEnd([]()
                   { 
                     ESP_WIFI_TAG_CONSOLE("[OTA] Succeed");
                     ESP_WIFI_TAG_LOG("[OTA] Succeed");
                   });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
                          uint32_t per = progress * 100 / total;
                          if (_otaArduinoPercent != per)
                          {
                            _otaArduinoPercent = per;
                            ESP_WIFI_TAG_CONSOLE("[OTA] Progress: %u%%", per);
                          }
                          /* Watch dog timer feed */
                          wdt_reset();
                        });

  ArduinoOTA.onError([&](ota_error_t error)
                     {
                       ESP_WIFI_TAG_CONSOLE("[OTA] Error[%u]: ", error);
                       ESP_WIFI_TAG_LOG("[OTA] Error[%u]: ", error);
                       if (error == OTA_AUTH_ERROR)
                       {
                         ESP_WIFI_CONSOLE("Auth Failed");
                       }
                       else if (error == OTA_BEGIN_ERROR)
                       {
                         ESP_WIFI_CONSOLE("Begin Failed");
                       }
                       else if (error == OTA_CONNECT_ERROR)
                       {
                         ESP_WIFI_CONSOLE("Connect Failed");
                       }
                       else if (error == OTA_RECEIVE_ERROR)
                       {
                         ESP_WIFI_CONSOLE("Receive Failed");
                       }
                       else if (error == OTA_END_ERROR)
                       {
                         ESP_WIFI_CONSOLE("End Failed");
                       }
                       OTAStop();
                     });

  ArduinoOTA.setHostname(ESPConfig.hostNameSTA().c_str());
  // ArduinoOTA.setPassword("1234");
  ArduinoOTA.begin();
}
#endif

void ESPWifiHandle::loop()
{
#if (defined MDNS_SERVICE_ENABLE) && (MDNS_SERVICE_ENABLE == 1) && (defined ESP8266)
  MDNS.update();
#endif

#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)  
  ArduinoOTA.handle();
#endif

#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1)
  _dnsServer.processNextRequest();
#endif
}

void ESPWifiHandle::begin(bool wifiON)
{
  WiFiMode_t wf_mode = WIFI_OFF;

  ESP_WIFI_TAG_CONSOLE("Begin()");
#if (defined SNTP_SERVICE_SYSTEM) && (SNTP_SERVICE_SYSTEM == 1)  
  // sntp service ON
  _sntp.begin();
#endif

#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
  EASY_NTP.onNTPSyncEvent([](struct timeval* val){
    EASY_SNTP_TAG_CONSOLE("[EASY_NTP][CB] Epoch/Unix system = %lu", ESPTime.now());
    ESPTime.set(val->tv_sec);    
    EASY_SNTP_TAG_CONSOLE("[EASY_NTP][CB] The UTC/GMT time is setting %s", ESPTime.toString().c_str());
    ESPTime.setSourceUpdate(ESPTimeSystem::ESP_RTC_SNTP_UPDATE);
  });
#endif

  // Register wifi event callback
  this->registerEventHandler();

  if (wifiON)
  {  
    ESP_WIFI_TAG_CONSOLE("Wifi start");
    /* Once WiFi.persistent(false) is called, WiFi.begin, 
     WiFi.disconnect, WiFi.softAP, or WiFi.softAPdisconnect 
     only changes the current in-memory Wi-Fi settings, 
     and does not affect the Wi-Fi settings stored in flash memory.

     Default is false
    */
    WiFi.persistent(true);
#ifdef ESP32
    /* Comment to default setting */
    // esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B);
    /*
      typedef enum {
          WIFI_POWER_19_5dBm = 78,// 19.5dBm
          WIFI_POWER_19dBm = 76,// 19dBm
          WIFI_POWER_18_5dBm = 74,// 18.5dBm
          WIFI_POWER_17dBm = 68,// 17dBm
          WIFI_POWER_15dBm = 60,// 15dBm
          WIFI_POWER_13dBm = 52,// 13dBm
          WIFI_POWER_11dBm = 44,// 11dBm
          WIFI_POWER_8_5dBm = 34,// 8.5dBm
          WIFI_POWER_7dBm = 28,// 7dBm
          WIFI_POWER_5dBm = 20,// 5dBm
          WIFI_POWER_2dBm = 8,// 2dBm
          WIFI_POWER_MINUS_1dBm = -4// -1dBm
      } wifi_power_t;
      */
    /* Comment to default setting */
    // WiFi.setTxPower(WIFI_POWER_19_5dBm);
#elif defined(ESP8266)
    WiFi.setPhyMode(WIFI_PHY_MODE_11B);
    WiFi.setOutputPower(20.5);
#endif

    /* sta and ap are disable*/
    if (ESPConfig.isDisableSTA() && ESPConfig.isDisableAP())
    {
      /* smart config is enable*/
      if (ESPConfig.smartCfgSTA())
      {
        wf_mode = WIFI_STA;
        ESP_WIFI_TAG_CONSOLE("Wifi Mode WIFI_STA using for smart config");
      }
      else
      {
        wf_mode = WIFI_OFF;
        ESP_WIFI_TAG_CONSOLE("Wifi Mode WIFI_OFF");
      }
    }
    /* sta and ap are enable*/
    else if (!ESPConfig.isDisableSTA() && !ESPConfig.isDisableAP())
    {
      wf_mode = WIFI_AP_STA;
      ESP_WIFI_TAG_CONSOLE("Wifi Mode WIFI_AP_STA");
    }
    /* Enable sta and Disable ap */
    else if (!ESPConfig.isDisableSTA() && ESPConfig.isDisableAP())
    {
      wf_mode = WIFI_STA;
      ESP_WIFI_TAG_CONSOLE("Wifi Mode WIFI_STA");
    }
    /* Disable sta and Enable ap */
    else
    {
      wf_mode = WIFI_AP;
      ESP_WIFI_TAG_CONSOLE("Wifi Mode WIFI_AP");
    }

    if (WIFI_OFF == wf_mode)
    {
      this->end();
    }
    else
    {
      WiFi.mode(wf_mode);
    }

    /* STA enable */
    if (!ESPConfig.isDisableSTA())
    {
#ifdef ESP8266
      String ChipID = String(ESP.getChipId(), HEX);
#elif defined(ESP32)
      String ChipID = String((uint32_t)(ESP.getEfuseMac() >> 16), HEX);
#endif
      ChipID.toUpperCase();
      String hostName = ESPConfig.hostNameSTA() + "_" + ChipID;
      ESP_WIFI_TAG_CONSOLE("SetHostname: %s", hostName.c_str());
      WiFi.setHostname(hostName.c_str());

      ESP_WIFI_TAG_CONSOLE("ssidSTA: %s", ESPConfig.ssidSTA().c_str());
      if (ESPConfig.ssidSTA().length() > 0) // not exist the access point 
      {
        if (!ESPConfig.dhcpSTA())
        {
          WiFi.config(ESPConfig.ipSTA(), ESPConfig.gwSTA(), ESPConfig.snSTA(), ESPConfig.dnsSTA());
          ESP_WIFI_TAG_CONSOLE("static IP setup");
          ESP_WIFI_TAG_CONSOLE("Ip: %s", ESPConfig.ipSTA().toString().c_str());
          ESP_WIFI_TAG_CONSOLE("Gw: %s", ESPConfig.gwSTA().toString().c_str());
          ESP_WIFI_TAG_CONSOLE("Sn: %s", ESPConfig.snSTA().toString().c_str());
          ESP_WIFI_TAG_CONSOLE("Dns: %s\r\n", ESPConfig.dnsSTA().toString().c_str());
        }

        this->connect(ESPConfig.ssidSTA().c_str(), ESPConfig.passSTA().c_str());
      }
      else
      {
        /* Smart config enable */
        ESP_WIFI_TAG_CONSOLE("[beginSmartConfig] = %u\r\n", ESPConfig.smartCfgSTA());
        if (ESPConfig.smartCfgSTA())
        {
          WiFi.beginSmartConfig();
        }
      }
    }

    /* AP enable */
    if (!ESPConfig.isDisableAP())
    {
#ifdef ESP8266
      String ChipID = String(ESP.getChipId(), HEX);
#elif defined(ESP32)
      String ChipID = String((uint32_t)(ESP.getEfuseMac() >> 16), HEX);
#endif
      ChipID.toUpperCase();
      String ssidAP = ESPConfig.ssidAP() + "_" + ChipID;
      WiFi.softAPConfig(ESPConfig.ipAP(), ESPConfig.ipAP(), ESPConfig.snAP());
      constexpr uint8_t PASSWORD_LENGTH_MIN = 8;
      if (ESPConfig.passAP().length() >= PASSWORD_LENGTH_MIN)
      {
        // using c_str() for esp32
        WiFi.softAP(ssidAP.c_str(), ESPConfig.passAP().c_str(), ESPConfig.channelAP(), ESPConfig.isHiddenAP());
      }
      else
      {
        // using c_str() for esp32
        WiFi.softAP(ssidAP.c_str());
      }

      ESP_WIFI_TAG_CONSOLE("AP IP address: %s\r\n", WiFi.softAPIP().toString().c_str());
    }
  }
  /* should be called after the wifi init */
#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)  
  this->onArduinoOTA();
#endif

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
  if (!ESPConfig.isDisableSTA())
  {
    this->onDDNSclient();
  }
#endif

#if (defined NBNS_SERVICE_ENABLE) && (NBNS_SERVICE_ENABLE == 1)
  if (!ESPConfig.isDisableSTA())
  {
    this->onNBNSService();
  }
#endif

#if (defined MDNS_SERVICE_ENABLE) && (MDNS_SERVICE_ENABLE == 1)
  if (!ESPConfig.isDisableSTA())
  {
    this->onMDNSService();
  }
#endif

#if (defined DNS_SERVER_ENABLE) && (DNS_SERVER_ENABLE == 1)
  if (!ESPConfig.isDisableAP())
  {
    this->onDNSServer();
  }
#endif
}


void ESPWifiHandle::OTAStart() { 
  _IsOTA = true; 
  if (_otaFunc) {
    _otaFunc(true);
  }
}

void ESPWifiHandle::OTAStop() { 
  _IsOTA = false; 
  if (_otaFunc) {
    _otaFunc(false);
  }
}

void ESPWifiHandle::end(void)
{
  if (WIFI_OFF != WiFi.getMode())
  {
    ESP_WIFI_TAG_CONSOLE("Wifi off\r\n");
    WiFi.disconnect(true);
    WiFi.enableAP(false);
  }
  WiFi.mode(WIFI_OFF);

  // force wifi off. We must be run the function to getmode() is really return WIFI_OFF
#ifdef ESP32
  esp_wifi_set_mode(WIFI_OFF);
#elif defined(ESP8266)
  // Refer example details
  // https://github.com/esp8266/Arduino/blob/master/libraries/esp8266/examples/interactive/interactive.ino
  wifi_set_opmode(WIFI_OFF);
#endif
}

void ESPWifiHandle::connect(const char *name, const char *pass)
{
  // We start by connecting to a WiFi network
  ESP_WIFI_TAG_CONSOLE("Connecting to %s", name);

  /* - Set whether module will attempt to reconnect
     to an access point in case it is disconnected.

     WiFi.setAutoReconnect(autoReconnect)
     - If parameter autoReconnect is set to true, then module will try to 
     reestablish lost connection to the AP. 
     - If set to false then module will stay disconnected.

     Note: running setAutoReconnect(true) when module is already disconnected 
     will not make it reconnect to the access point. Instead reconnect() 
     should be used.
    */
  WiFi.setAutoReconnect(false);
  constexpr uint8_t PASSWORD_LENGTH_MIN = 8;
  if (strlen(pass) >= PASSWORD_LENGTH_MIN)
  {
    WiFi.begin(name, pass);
  }
  else
  {
    WiFi.begin(name);
  }

  ESP_WIFI_TAG_CONSOLE("Wait for WiFi...");
}

/* make json wifi network
{
  "status":"ok",
  "mgs":"Network OK",
  "sta_network":
  [
      {"ssid":"Quyen_2.4G","rssi":50,"secure":1}
  ]
}
*/
int ESPWifiHandle::ssidScan(String &json)
{
  boolean removeDuplicateAPs = true;
  int minimumQuality = -1;

  int scanCount = WiFi.scanComplete();
  ESP_WIFI_TAG_CONSOLE("WiFi scanComplete result = %d", scanCount);
  if (scanCount >= 1)
  {
    ESP_WIFI_TAG_CONSOLE("scanNetworks make json");
    std::unique_ptr<int[]> buf(new(std::nothrow) int[scanCount]);

    if (!buf)
    {
      // clean up ram
      WiFi.scanDelete();
      return -2; // catch as WIFI_SCAN_FAILED
    }

    int* indices = buf.get();
    //sort networks
    for (int i = 0; i < scanCount; i++)
    {
      indices[i] = i;
    }

    // RSSI SORT
    // old sort
    for (int i = 0; i < scanCount; i++)
    {
      for (int j = i + 1; j < scanCount; j++)
      {
        if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i]))
        {
          std::swap(indices[i], indices[j]);
        }
      }
    }

    // remove duplicates ( must be RSSI sorted )
    if (removeDuplicateAPs)
    {
      String cssid;
      for (int i = 0; i < scanCount; i++)
      {
        if (indices[i] != -1)
        {
          cssid = WiFi.SSID(indices[i]);
          for (int j = i + 1; j < scanCount; j++)
          {
            if (cssid == WiFi.SSID(indices[j]))
            {
              indices[j] = -1; // set dup aps to index -1
            }
          }
        }
      }
    }

    /*
    {
        "status":"ok",
        "mgs":"Network OK",
        "sta_network":
        [
            {"ssid":"Quyen_2.4G","rssi":50,"secure":1}
        ]
    }
    */
    json = "{\"status\":\"ok\",\"mgs\":\"Network OK\",\"sta_network\":[";
    //display networks in page
    for (int i = 0; i < scanCount; i++)
    {
      if (indices[i] != -1)
      {
        int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));
        if (minimumQuality == -1 || minimumQuality < quality)
        {
          if (i)
            json += ",";
          json += "{";
          json += "\"ssid\":\"" + String(WiFi.SSID(indices[i])) + "\"";
          json += ",\"rssi\":" + String(quality);

          // ESP32
          // typedef enum {
          //     WIFI_AUTH_OPEN = 0,         /**< authenticate mode : open */
          //     WIFI_AUTH_WEP,              /**< authenticate mode : WEP */
          //     WIFI_AUTH_WPA_PSK,          /**< authenticate mode : WPA_PSK */
          //     WIFI_AUTH_WPA2_PSK,         /**< authenticate mode : WPA2_PSK */
          //     WIFI_AUTH_WPA_WPA2_PSK,     /**< authenticate mode : WPA_WPA2_PSK */
          //     WIFI_AUTH_WPA2_ENTERPRISE,  /**< authenticate mode : WPA2_ENTERPRISE */
          //     WIFI_AUTH_MAX
          // } wifi_auth_mode_t;

          // ESP8266
          // enum wl_enc_type {  /* Values map to 802.11 encryption suites... */
          //         ENC_TYPE_WEP  = 5,
          //         ENC_TYPE_TKIP = 2,
          //         ENC_TYPE_CCMP = 4,
          //         /* ... except these two, 7 and 8 are reserved in 802.11-2007 */
          //         ENC_TYPE_NONE = 7,
          //         ENC_TYPE_AUTO = 8
          // };
          json += ",\"secure\":" + String(WiFi.encryptionType(indices[i]));
          json += "}";
        }
        else
        {
          ESP_WIFI_TAG_CONSOLE("Skipping due to quality");
        }
      } // if (indices[i] != -1)
    }   // for (int i = 0; i < scanCount; i++)

    json += "]}";

    // clean up ram
    WiFi.scanDelete();
  } // if (scanCount >= 0)
  else
  {
    json = "{\"status\":\"error\",\"mgs\":\"Not find any Network\"}";
  }

  return scanCount;
}

/* Convert to %*/
int ESPWifiHandle::getRSSIasQuality(int RSSI)
{
  int quality = 0;
  if (RSSI <= -100)
  {
    quality = 0;
  }
  else if (RSSI >= -50)
  {
    quality = 100;
  }
  else
  {
    quality = 2 * (RSSI + 100);
  }
  return quality;
}

ESPWifiHandle ESPWifi;
