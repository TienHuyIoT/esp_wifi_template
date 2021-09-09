#ifdef ESP32
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
#endif

#include "app_config.h"
#include "hth_console_dbg.h"
#include "hth_esp_sys_data.h"
#include "hth_esp_sys_rtc.h"
#include "hth_esp_wifi.h"

#define SNTP_CONSOLE_PORT CONSOLE_PORT
#define SNTP_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)
#define SNTP_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[SNTP]", __VA_ARGS__)

#define ESP_WIFI_PORT CONSOLE_PORT
#define ESP_WIFI_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)
#define ESP_WIFI_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[WIFI]", __VA_ARGS__)

#ifdef ESP32
static void sntp_sync_time_cb(struct timeval *tv) {
    SNTP_TAG_CONSOLE("settimeofday(SNTP)");
#elif defined(ESP8266)
static void sntp_sync_time_cb(bool from_sntp /* <= this parameter is optional */) {
    SNTP_TAG_CONSOLE("settimeofday(%s)", from_sntp ? "SNTP" : "USER");
#endif 
    time_t now = time(nullptr);
    const tm* tm = localtime(&now);
    char buf[64];
    /** The same way using with esp32 
     * RTC_CONSOLE_PORT.println(&tmStruct, "\r\nTime: %A, %B %d %Y %H:%M:%S");
     * */
    strftime(buf, 64, "%A, %B %d %Y %H:%M:%S", tm);
    SNTP_TAG_CONSOLE("Time: %s", buf);

    HTH_sysTime.setSourceUpdate(flatform_rtc::RTC_SNTP_UPDATE);
}

hth_esp_sntp::hth_esp_sntp() {}
hth_esp_sntp::~hth_esp_sntp() {}

void hth_esp_sntp::begin()
{
    SNTP_TAG_CONSOLE("Configure Time Server");
#ifdef ESP32
    sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
    configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer1, _ntpServer2);
    // Using callback event instead to check sntp status
    // while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {};
#elif defined(ESP8266)
    // install callback - called when settimeofday is called (by SNTP or user)
    // once enabled (by DHCP), SNTP is updated every hour by default
    // ** optional boolean in callback function is true when triggered by SNTP **
    settimeofday_cb(sntp_sync_time_cb);
    configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer1, _ntpServer2);
#endif
}

/* hth_esp_wifi class --------------------------------------------------------
 -----------------------------------------------------------------------------
 ---------------------------------------------------------------------------*/
hth_esp_wifi::hth_esp_wifi(/* args */)
#if (defined SNTP_SERVICE_ENABLE) && (SNTP_SERVICE_ENABLE == 1)  
    : _sntp(new hth_esp_sntp())
#endif
{
}

hth_esp_wifi::~hth_esp_wifi()
{
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
  if (_ddnsClient)
  {
    delete _ddnsClient;
  }
#endif

#if (defined SNTP_SERVICE_ENABLE) && (SNTP_SERVICE_ENABLE == 1)  
  delete _sntp;
#endif
}

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
AsyncEasyDDNSClass* hth_esp_wifi::_ddnsClient = nullptr;
#endif

void hth_esp_wifi::eventSSetup()
{
#ifdef ESP32
#elif defined(ESP8266)
#endif
}

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
void hth_esp_wifi::onDDNSclient()
{
  if (WFDataFile.disableDDNS())
  {
      ESP_WIFI_TAG_CONSOLE("[DDNS] client disable");
      return;
  }

  if (_ddnsClient)
  {
    return;
  }
  
  _ddnsClient = new AsyncEasyDDNSClass();
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
  _ddnsClient->service(WFDataFile.serviceDDNS());

  /*
    For DDNS Providers where you get a token:
      Use this: _ddnsClient->client("domain", "token");
    
    For DDNS Providers where you get username and password: ( Leave the password field empty "" if not required )
      Use this: _ddnsClient->client("domain", "username", "password");
  */
  _ddnsClient->client(WFDataFile.domainDDNS(), WFDataFile.userDDNS(), WFDataFile.passDDNS());

  // Get Notified when your IP changes
  _ddnsClient->onUpdate([&](const char* oldIP, const char* newIP){
    ESP_WIFI_TAG_CONSOLE("[DDNS] AsyncEasyDDNS - IP Change Detected: %s", newIP);
  });

  constexpr uint8_t DDNS_SYNC_TIME_MIN = 10; // should not zero
  constexpr uint8_t DDNS_SYNC_TIME_MAX = 60; // unlimited
  if (WFDataFile.syncTimeDDNS() < DDNS_SYNC_TIME_MIN)
  {
    WFDataFile.syncTimeDDNSSet(DDNS_SYNC_TIME_MIN);
  }

  if (WFDataFile.syncTimeDDNS() > DDNS_SYNC_TIME_MAX)
  {
    WFDataFile.syncTimeDDNSSet(DDNS_SYNC_TIME_MAX);
  }

  _ddnsTicker.attach(WFDataFile.syncTimeDDNS(), [](){
    if (!WFDataFile.disableDDNS())
    {
      _ddnsClient->update();
    }  
  }); 
}
#endif

/* Should be called on wifi event gotIP */
#if (defined MDNS_SERVICE_ENABLE) && (MDNS_SERVICE_ENABLE == 1)
void hth_esp_wifi::onMDNSService()
{
  // Set up mDNS responder:
  // - first argument is the domain name, in this example
  //   the fully-qualified domain name is "esp32.local"
  // - second argument is the IP address to advertise
  //   we send our IP address on the WiFi network
  if (!MDNS.begin(WFDataFile.hostNameSTA().c_str()))
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

/** Should be called on wifi event gotIP*/
void hth_esp_wifi::onNBNSService()
{
#if (defined NBNS_SERVICE_ENABLE) && (NBNS_SERVICE_ENABLE == 1)
  NBNS.begin(WFDataFile.hostNameSTA().c_str());
#endif
}

/** Should be called after init wifi */
void hth_esp_wifi::onArduinoOTA()
{
#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)
  ArduinoOTA.onStart(
      []()
      {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
          type = "sketch";
        }
        else // U_SPIFFS
        {
          type = "filesystem";
          NAND_FS_SYSTEM.end();
        }

        /** NOTE: if updating SPIFFS this would be the place
         *  to unmount SPIFFS using SPIFFS.end() */
        ESP_WIFI_TAG_CONSOLE("[OTA] Start updating %s", type.c_str());
      });

  ArduinoOTA.onEnd([]()
                   { ESP_WIFI_TAG_CONSOLE("[OTA] End"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
                          ESP_WIFI_TAG_CONSOLE("[OTA] Progress: %u%%", (progress / (total / 100)));
                          /* Watch dog timer feed */
                          // hw_wdt_feed();
                        });

  ArduinoOTA.onError([](ota_error_t error)
                     {
                       ESP_WIFI_TAG_CONSOLE("[OTA] Error[%u]: ", error);
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
                     });

  ArduinoOTA.setHostname(WFDataFile.hostNameSTA().c_str());
  ArduinoOTA.setPassword("1234");
  ArduinoOTA.begin();
#endif
}

void hth_esp_wifi::loop()
{
#if (defined MDNS_SERVICE_ENABLE) && (MDNS_SERVICE_ENABLE == 1) && (defined ESP8266)
  MDNS.update();
#endif

#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)  
  ArduinoOTA.handle();
#endif
}

void hth_esp_wifi::begin()
{
  WiFiMode_t wf_mode = WIFI_OFF;

#if (defined SNTP_SERVICE_ENABLE) && (SNTP_SERVICE_ENABLE == 1)  
  // sntp service ON
  _sntp->begin();
#endif

  /* Once WiFi.persistent(false) is called, WiFi.begin, 
     WiFi.disconnect, WiFi.softAP, or WiFi.softAPdisconnect 
     only changes the current in-memory Wi-Fi settings, 
     and does not affect the Wi-Fi settings stored in flash memory.

     Default is false
    */
  WiFi.persistent(false);
#ifdef ESP32
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B);
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
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
#elif defined(ESP8266)
  WiFi.setPhyMode(WIFI_PHY_MODE_11B);
  WiFi.setOutputPower(20.5);
#endif

  /* sta and ap are disable*/
  if (WFDataFile.isDisableSTA() && WFDataFile.isDisableAP())
  {
    /* smart config is enable*/
    if (WFDataFile.smartCfgSTA())
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
  else if (!WFDataFile.isDisableSTA() && !WFDataFile.isDisableAP())
  {
    wf_mode = WIFI_AP_STA;
    ESP_WIFI_TAG_CONSOLE("Wifi Mode WIFI_AP_STA");
  }
  /* Enable sta and Diasble ap */
  else if (!WFDataFile.isDisableSTA() && WFDataFile.isDisableAP())
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
  if (!WFDataFile.isDisableSTA())
  {
    if (WFDataFile.ssidSTA().length() > 0)
    {
      if (!WFDataFile.dhcpSTA())
      {
        WiFi.config(WFDataFile.ipSTA(), WFDataFile.gwSTA(), WFDataFile.snSTA(), WFDataFile.dnsSTA());
        ESP_WIFI_TAG_CONSOLE("static IP setup");
        ESP_WIFI_TAG_CONSOLE("Ip: %s", WFDataFile.ipSTA().toString().c_str());
        ESP_WIFI_TAG_CONSOLE("Gw: %s", WFDataFile.gwSTA().toString().c_str());
        ESP_WIFI_TAG_CONSOLE("Sn: %s", WFDataFile.snSTA().toString().c_str());
        ESP_WIFI_TAG_CONSOLE("Dns: %s\r\n", WFDataFile.dnsSTA().toString().c_str());
      }

      this->connect(WFDataFile.ssidSTA().c_str(), WFDataFile.passSTA().c_str());
    }
    else
    {
      /* Smart config enable */
      ESP_WIFI_TAG_CONSOLE("[beginSmartConfig] = %u\r\n", WFDataFile.smartCfgSTA());
      if (WFDataFile.smartCfgSTA())
      {
        WiFi.beginSmartConfig();
      }
    }
  }

  /* AP enable */
  if (!WFDataFile.isDisableAP())
  {
#ifdef ESP8266
    String ChipID = String(ESP.getChipId(), HEX);
#elif defined(ESP32)
    String ChipID = String((uint32_t)(ESP.getEfuseMac() >> 16), HEX);
#endif
    ChipID.toUpperCase();
    String ssidAP = WFDataFile.ssidAP() + "_" + ChipID;
    WiFi.softAPConfig(WFDataFile.ipAP(), WFDataFile.ipAP(), WFDataFile.snAP());
    if (WFDataFile.passAP().length() >= 8)
    {
      // using c_str() for esp32
      WiFi.softAP(ssidAP.c_str(), WFDataFile.passAP().c_str(), WFDataFile.channelAP(), WFDataFile.isHiddenAP());
    }
    else
    {
      // using c_str() for esp32
      WiFi.softAP(ssidAP.c_str());
    }

    ESP_WIFI_TAG_CONSOLE("AP IP address: %s\r\n", WiFi.softAPIP().toString().c_str());
  }

  /* should be called after the wifi init */
  this->onArduinoOTA();

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)  
  this->onDDNSclient();
#endif

  this->onNBNSService();

#if (defined MDNS_SERVICE_ENABLE) && (MDNS_SERVICE_ENABLE == 1)  
  this->onMDNSService();
#endif
}

void hth_esp_wifi::end(void)
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

void hth_esp_wifi::connect(const char *name, const char *pass)
{
  // We start by connecting to a WiFi network
  ESP_WIFI_TAG_CONSOLE("Connecting to %s", name);

  /* Set whether module will attempt to reconnect
     to an access point in case it is disconnected. 
    */
  WiFi.setAutoReconnect(true);
  if (strlen(pass) >= 8)
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
int hth_esp_wifi::ssidScan(String &json)
{
  boolean removeDuplicateAPs = true;
  int minimumQuality = -1;

  int n = WiFi.scanComplete();
  ESP_WIFI_TAG_CONSOLE("WiFi scanComplete result = %d", n);
  if (n >= 1)
  {
    ESP_WIFI_TAG_CONSOLE("scanNetworks make json");
    //sort networks
    int indices[n];
    for (int i = 0; i < n; i++)
    {
      indices[i] = i;
    }

    // RSSI SORT
    // old sort
    for (int i = 0; i < n; i++)
    {
      for (int j = i + 1; j < n; j++)
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
      for (int i = 0; i < n; i++)
      {
        if (indices[i] != -1)
        {
          cssid = WiFi.SSID(indices[i]);
          for (int j = i + 1; j < n; j++)
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
    for (int i = 0; i < n; i++)
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

          // typedef enum {
          //     WIFI_AUTH_OPEN = 0,         /**< authenticate mode : open */
          //     WIFI_AUTH_WEP,              /**< authenticate mode : WEP */
          //     WIFI_AUTH_WPA_PSK,          /**< authenticate mode : WPA_PSK */
          //     WIFI_AUTH_WPA2_PSK,         /**< authenticate mode : WPA2_PSK */
          //     WIFI_AUTH_WPA_WPA2_PSK,     /**< authenticate mode : WPA_WPA2_PSK */
          //     WIFI_AUTH_WPA2_ENTERPRISE,  /**< authenticate mode : WPA2_ENTERPRISE */
          //     WIFI_AUTH_MAX
          // } wifi_auth_mode_t;
          json += ",\"secure\":" + String(WiFi.encryptionType(indices[i]));
          json += "}";
        }
        else
        {
          ESP_WIFI_TAG_CONSOLE("Skipping due to quality");
        }
      } // if (indices[i] != -1)
    }   // for (int i = 0; i < n; i++)

    json += "]}";

    // clean up ram
    WiFi.scanDelete();
  } // if (n >= 0)
  else
  {
    json = "{\"status\":\"error\",\"mgs\":\"Not find any Network\"}";
  }

  return n;
}

/* Convert to %*/
int hth_esp_wifi::getRSSIasQuality(int RSSI)
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

hth_esp_wifi HTH_EspWifi;
