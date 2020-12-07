#include "WiFiType.h"
#include "wifi_data_file.h"

#define ESP_WIFI_PORT Serial
#define ESP_WIFI_PRINTF(f_, ...) ESP_WIFI_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void wifi_init(void)
{
  char ssid[Df_LengSsid + 1];
  wifi_file_json_t *g_wifi_cfg;
  WiFiMode_t wf_mode = WIFI_OFF;

  g_wifi_cfg = wifi_info_get();

  /* Once WiFi.persistent(false) is called, WiFi.begin, 
     WiFi.disconnect, WiFi.softAP, or WiFi.softAPdisconnect 
     only changes the current in-memory Wi-Fi settings, 
     and does not affect the Wi-Fi settings stored in flash memory.
    */
  WiFi.persistent(false);

  esp_wifi_set_protocol(ESP_IF_WIFI_AP, WIFI_PROTOCOL_11B);
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

  /* Disable sta and ap */
  if(g_wifi_cfg->sta.Dis && g_wifi_cfg->ap.Dis)
  {
    /* Enable smart config */
    if(g_wifi_cfg->sta.SmCfg)
    {
      wf_mode = WIFI_STA;
      ESP_WIFI_PRINTF("\r\nWifi Mode WIFI_STA using for smart config");
    }
    else
    {
      wf_mode = WIFI_OFF;
      ESP_WIFI_PRINTF("\r\nWifi Mode WIFI_OFF");
    }    
  }
  /* Enable sta and ap */
  else if (!g_wifi_cfg->sta.Dis && !g_wifi_cfg->ap.Dis)
  {
    wf_mode = WIFI_AP_STA;
    ESP_WIFI_PRINTF("\r\nWifi Mode WIFI_AP_STA");
  }
  /* Enable sta and Diasble ap */
  else if (!g_wifi_cfg->sta.Dis && g_wifi_cfg->ap.Dis)
  {
    wf_mode = WIFI_STA;
    ESP_WIFI_PRINTF("\r\nWifi Mode WIFI_STA");
  }
  /* Disable sta and Enable ap */
  else
  {
    wf_mode = WIFI_AP;
    ESP_WIFI_PRINTF("\r\nWifi Mode WIFI_AP");
  }
  
  if (WIFI_OFF == wf_mode)
  {
    wifi_off();
  }
  else
  {
    WiFi.mode(wf_mode);
  }  

  /* STA enable */
  if (!g_wifi_cfg->sta.Dis)
  {
    if (strlen(g_wifi_cfg->sta.ssid) > 0)
    {
      if (!g_wifi_cfg->sta.Dhcp)
      {
        WiFi.config(g_wifi_cfg->sta.Ip, g_wifi_cfg->sta.Gw, g_wifi_cfg->sta.Sn, g_wifi_cfg->sta.Dns);
        ESP_WIFI_PRINTF("\r\nstatic IP enable");
        ESP_WIFI_PRINTF("\r\nIp: %s", g_wifi_cfg->sta.Ip.toString().c_str());
        ESP_WIFI_PRINTF("\r\nGw: %s", g_wifi_cfg->sta.Gw.toString().c_str());
        ESP_WIFI_PRINTF("\r\nSn: %s", g_wifi_cfg->sta.Sn.toString().c_str());
        ESP_WIFI_PRINTF("\r\nDns: %s\r\n", g_wifi_cfg->sta.Dns.toString().c_str());
      }

      wifi_setup(g_wifi_cfg->sta.ssid, g_wifi_cfg->sta.pass); 
    }
    else
    {
      /* Smart config enable */
      ESP_WIFI_PRINTF("\r\n[beginSmartConfig] = %u\r\n", g_wifi_cfg->sta.SmCfg);
      if(g_wifi_cfg->sta.SmCfg)
      {
        WiFi.beginSmartConfig();
      }
    }    
  }

  /* AP enable */
  if (!g_wifi_cfg->ap.Dis)
  {
    snprintf(ssid, Df_LengSsid + 1, "%s_%08X", g_wifi_cfg->ap.ssid, (uint32_t)ESP.getEfuseMac());
    WiFi.softAPConfig(g_wifi_cfg->ap.Ip, g_wifi_cfg->ap.Ip, g_wifi_cfg->ap.Sn);
    if (strlen(g_wifi_cfg->ap.pass) >= 8)
    {
      WiFi.softAP(ssid, g_wifi_cfg->ap.pass, g_wifi_cfg->ap.Chanel, g_wifi_cfg->ap.Hidden);
    }
    else
    {
      WiFi.softAP(g_wifi_cfg->ap.ssid);
    }

    ESP_WIFI_PRINTF("\r\nAP IP address: %s\r\n", WiFi.softAPIP().toString().c_str());
  }  
}

// typedef enum {
//     WIFI_MODE_NULL = 0,  /**< null mode */
//     WIFI_MODE_STA,       /**< WiFi station mode */
//     WIFI_MODE_AP,        /**< WiFi soft-AP mode */
//     WIFI_MODE_APSTA,     /**< WiFi station + soft-AP mode */
//     WIFI_MODE_MAX
// } wifi_mode_t;
void wifi_off()
{
  if (WIFI_OFF != WiFi.getMode())
  {
    ESP_WIFI_PRINTF("\r\nWifi off\r\n");
    WiFi.disconnect(true);
    WiFi.enableAP(false);    
  }
  WiFi.mode(WIFI_OFF);
  esp_wifi_set_mode(WIFI_OFF); // force wifi off. We must be run the function to getmode is really return WIFI_OFF
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
int esp_ssid_scan(String &json)
{
  boolean _removeDuplicateAPs = true;
  int _minimumQuality = -1;
  int n = WiFi.scanNetworks();
  
  if (n != 0)
  {
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
    if (_removeDuplicateAPs)
    {
      String cssid;
      for (int i = 0; i < n; i++)
      {
        if (indices[i] != -1){
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
        if (_minimumQuality == -1 || _minimumQuality < quality)
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
          ESP_WIFI_PRINTF("Skipping due to quality");
        }
      } // if (indices[i] != -1)
    } // for (int i = 0; i < n; i++)

    json += "]}";
  } // if (n != 0)

  // clean up ram
  WiFi.scanDelete();

  return n;
}