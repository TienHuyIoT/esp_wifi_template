#define ESP_WIFI_PORT Serial
#define ESP_WIFI_PRINTF(f_, ...) ESP_WIFI_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void wifi_init(void) {
    char ssid[Df_LengSsid + 1];
    wifi_file_json_t* g_wifi_cfg;
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

    if (strlen(g_wifi_cfg->sta.ssid) > 0)
    {
        WiFi.mode(WIFI_AP_STA);
        if (g_wifi_cfg->sta.SaticIp)
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
        WiFi.mode(WIFI_AP);
    }
       
    snprintf(ssid, Df_LengSsid + 1, "%s_%08X", g_wifi_cfg->ap.ssid, (uint32_t)ESP.getEfuseMac());
    WiFi.softAPConfig(g_wifi_cfg->ap.Ip, g_wifi_cfg->ap.Ip, g_wifi_cfg->ap.Sn);
    if (strlen(g_wifi_cfg->ap.pass) >= 8) {
        WiFi.softAP(ssid, g_wifi_cfg->ap.pass, g_wifi_cfg->ap.Chanel, g_wifi_cfg->ap.Hidden);
    }
    else
    {
        WiFi.softAP(g_wifi_cfg->ap.ssid);
    }
    
    IPAddress myIP = WiFi.softAPIP();
    //In ra AP IP
    ESP_WIFI_PRINTF("\r\nAP IP address: %s\r\n", myIP.toString().c_str());
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
        WiFi.mode(WIFI_OFF);
        esp_wifi_set_mode(WIFI_OFF); // force wifi off. We must be run the function to getmode is really return WIFI_OFF
    }
}