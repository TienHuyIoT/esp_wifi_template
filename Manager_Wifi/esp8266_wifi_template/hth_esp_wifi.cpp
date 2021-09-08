#ifdef ESP32
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiType.h>
#include <ESPmDNS.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <user_interface.h>
#endif

#include "hth_esp_sys_data.h"
#include "hth_console_dbg.h"
#include "hth_esp_wifi.hpp"
#include "hth_esp_sys_data.h"

#define ESP_WIFI_PORT CONSOLE_PORT
#define ESP_WIFI_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)
#define ESP_WIFI_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[WIFI]", __VA_ARGS__)

hth_esp_wifi::hth_esp_wifi(/* args */)
{
}

hth_esp_wifi::~hth_esp_wifi()
{
}

void hth_esp_wifi::begin()
{
  WiFiMode_t wf_mode = WIFI_OFF;

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
