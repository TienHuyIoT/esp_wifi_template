#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "app_config.h"
#include "eeprom_data.h"
#include "rtc_data_file.h"
#include "wifi_data_file.h"
#include "server_data_process.h"

#define SERVER_DATA_PORT Serial
#define SERVER_DATA_PRINTF(f_, ...) SERVER_DATA_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void server_data_get_process(AsyncWebServerRequest *request)
{
    uint8_t cb = 0;
    /* param wifi get 
     * /get?argName=arg(0)
     * using argName(0) = "param_wifi" is parameter to get wifi information 
     * using arg(0) is parameter to find callback_post
     */
    AsyncWebParameter* p = request->getParam(0);
    if (p->name() == "param_wifi")
    {              
        for (uint8_t i = 0; i < DATA_GET_HANDLE_NUM; ++i)
        {
            if (p->value() == client_get_handle[i].path_arg)
            {
                SERVER_DATA_PRINTF("\r\nProcess get callback [%u]", i);
                client_get_handle[i].cb(request);
                cb = 1;
                break;
            }
        }
    }
    /* non process callback, so return arg parse */
    if (!cb)
    {
        String arg_str = "";
        print_handlerequest(request, arg_str);
        request->send(200, "text/html", arg_str);
    }
    
}

void server_data_post_process(AsyncWebServerRequest *request)
{
    uint8_t cb = 0;
    /* param wifi post 
     * /post?
     * argName(0) = arg(0)
     * using argName(0) is parameter to find callback_post
     * using arg(0) is data
     */
    AsyncWebParameter* p = request->getParam(0);
    for (uint8_t i = 0; i < DATA_POST_HANDLE_NUM; ++i)
    {
        if (p->name() == client_post_handle[i].path_arg)
        {
            SERVER_DATA_PRINTF("\r\nProcess post callback [%u]", i);
            client_post_handle[i].cb(request);
            cb = 1;
            break;
        }
    }

    /* non process callback, so return arg parse */
    if (!cb)
    {
        String arg_str = "";
        print_handlerequest(request, arg_str);
        request->send(200, "text/html", arg_str);
    }
}

/*---------------------------------------------------------------------*
 *----------------------------data get process-------------------------*
 *---------------------------------------------------------------------*/
void sta_ap_info_get(AsyncWebServerRequest *request)
{
    String json_network;
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();

    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();
    root["ap_ssid"].set(g_wifi_cfg->ap.ssid);
    root["ap_ip_adress"].set(g_wifi_cfg->ap.Ip.toString());
    root["sta_ssid"].set(g_wifi_cfg->sta.ssid);   

    if(!g_wifi_cfg->sta.Dhcp) 
    {
        root["sta_ip_dhcp"].set("Disable");
    }
    else
    {
        root["sta_ip_dhcp"].set("Enable");
    }    
    
    if (WL_CONNECTED == WiFi.status())
    {
        root["sta_ip_address"].set(WiFi.localIP().toString());
        root["sta_status"].set("Connected");
    }
    else
    {
        if(!g_wifi_cfg->sta.Dhcp) 
        {
            root["sta_ip_address"].set(g_wifi_cfg->sta.Ip.toString());
        }
        else
        {
            root["sta_ip_address"].set("0.0.0.0");
        }
        
        root["sta_status"].set("Disconnect");
    }
    
    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

/* Get json sta_network */
void sta_network_get(AsyncWebServerRequest *request)
{
    String json_network = "{\"status\":\"error\",\"mgs\":\"No network\"}";
    esp_ssid_scan(json_network);
    request->send(200, "text/json", json_network);
}
void sta_setting_get(AsyncWebServerRequest *request) 
{
    String json_network;
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["sta_ssid"].set(g_wifi_cfg->sta.ssid);
    root["sta_pass"].set(g_wifi_cfg->sta.pass); 
    if (WL_CONNECTED == WiFi.status())
    {
        root["sta_ip"].set(WiFi.localIP().toString());
        root["sta_gw"].set(WiFi.gatewayIP().toString());
        root["sta_sm"].set(WiFi.subnetMask().toString());
        root["sta_dns"].set(WiFi.dnsIP().toString());   
    }
    else
    {
        if(!g_wifi_cfg->sta.Dhcp)
        {
            root["sta_ip"].set(g_wifi_cfg->sta.Ip.toString());
            root["sta_gw"].set(g_wifi_cfg->sta.Gw.toString());
            root["sta_sm"].set(g_wifi_cfg->sta.Sn.toString());
            root["sta_dns"].set(g_wifi_cfg->sta.Dns.toString());
        }
        else
        {
            root["sta_ip"].set("0.0.0.0");
            root["sta_gw"].set("0.0.0.0");
            root["sta_sm"].set("0.0.0.0");
            root["sta_dns"].set("0.0.0.0");
        }
        
    }
    
    root["sta_dhcp"].set(g_wifi_cfg->sta.Dhcp);
    root["sta_on"].set(!g_wifi_cfg->sta.Dis);  
    root["udp_port"].set(g_wifi_cfg->UDPPort);
    root["tcp_port"].set(g_wifi_cfg->TCPPort);
    root["ws_port"].set(g_wifi_cfg->WSPort);

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void ap_setting_get(AsyncWebServerRequest *request) 
{
    String json_network;
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["ap_ssid"].set(g_wifi_cfg->ap.ssid);
    root["ap_pass"].set(g_wifi_cfg->ap.pass);
    root["ap_on"].set(!g_wifi_cfg->ap.Dis);  
    root["ap_channel"].set(g_wifi_cfg->ap.Chanel);
    root["ap_hidden"].set(g_wifi_cfg->ap.Hidden);

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void device_address_get(AsyncWebServerRequest *request)
{
    String json_network;
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["device_name"].set(g_wifi_cfg->addr.device_name);
    root["device_addr"].set(g_wifi_cfg->addr.device_addr);

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void time_setting_get(AsyncWebServerRequest *request)
{    
    String json_network;
    rtc_time_t rtc;

    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();  

    if (rtc_get(&rtc))
    {            
        char time[9];
        char date[11];

        snprintf(time, 9, "%02u:%02u:%02u", rtc.hour, rtc.min, rtc.sec);
        snprintf(date, 11, "%04u/%02u/%02u", rtc.year, rtc.mon, rtc.mday);
         
        root["time"].set(time);
        root["date"].set(date);        
    }
    else
    {
        root["time"].set("00:00:00");
        root["date"].set("2000/01/01");
    }

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

/* /get?param_wifi=fw_version */
void fw_version_get(AsyncWebServerRequest *request)
{
    char buf[30];
    snprintf(buf, 30, "{\"fw_version\":\"%u.%u.%u\"}",
            FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_BUILD);
    request->send(200, "text/json", buf);
}

/* /get?param_wifi=restart */
void restart_device_get(AsyncWebServerRequest *request)
{
    request->send(200, "text/json", "Reset OK");
    log_report(LOG_REPORT_RESET, (char *)"Web reset3");
    esp_reset_enable();
}

/* /get?param_wifi=heap_temperature */
void heap_temperature_get(AsyncWebServerRequest *request)
{
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(esp32_internal_temp());
    json += ", \"gpio\":1";
    json += "}";
    request->send(200, "text/json", json);
    json = String();
}

/* /get?param_wifi=activated&cmd=X 
 * [X] = 0: InActive
 * [X] = 1: Active
 * [X] > 1: Read status
 */
void activated_get(AsyncWebServerRequest *request)
{    
    if (request->argName(1) == "cmd")
    {
        uint8_t cmd = atoi(request->arg(1).c_str());
        if (0 == cmd || 1 == cmd)
        {
            if (1 == cmd)
            {
                eeprom_device_active();
            }
                
            if (0 == cmd)
            {
                eeprom_device_inactive();
            }
        }
        request->send(200, "text/html", "Vaule: " + String(eeprom_device_is_activated()));
    }    
}

void format_sd_card_get(AsyncWebServerRequest *request)
{
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)    
    sd_format(SD_FS_SYSTEM, "/");
    request->send(200, "text/html", "Format SD card Succeed");
#else
    request->send(200, "text/html", "No Support SD Card");
#endif        
}

/*---------------------------------------------------------------------*
 *----------------------------data post process------------------------*
 *---------------------------------------------------------------------*/
void sta_ap_info_post(AsyncWebServerRequest *request)
{
}

/* Post json sta_network 
{
"sta_ssid": "Quyen_2.4G",
"sta_ssid": "12345679",
"access_code": "1234"
}
*/
void sta_network_post(AsyncWebServerRequest *request) 
{
    wifi_file_json_t *g_wifi_cfg;
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_PRINTF("JSON parsing failed!");
        return;
    }

    g_wifi_cfg = wifi_info_get();

    if("1234" == root["access_code"])
    {
        request->send(200, "text/json", "Wifi Setting Succeed");

        root["sta_ssid"].as<String>().toCharArray(g_wifi_cfg->sta.ssid, Df_LengSsid + 1);
        root["sta_pass"].as<String>().toCharArray(g_wifi_cfg->sta.pass, Df_LengPass + 1);
        SERVER_DATA_PRINTF("\r\nSSID: %s", g_wifi_cfg->sta.ssid);
        SERVER_DATA_PRINTF("\r\nPASS: %s\r\n", g_wifi_cfg->sta.pass);        

        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable();
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void sta_setting_post(AsyncWebServerRequest *request)
{
    wifi_file_json_t *g_wifi_cfg;  
    AsyncWebParameter* p = request->getParam(0);  

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_PRINTF("JSON parsing failed!");
        return;
    }

    g_wifi_cfg = wifi_info_get();

    if("1234" == root["access_code"])
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");

        root["sta_ssid"].as<String>().toCharArray(g_wifi_cfg->sta.ssid, Df_LengSsid + 1);
        root["sta_pass"].as<String>().toCharArray(g_wifi_cfg->sta.pass, Df_LengPass + 1);
        g_wifi_cfg->sta.Ip.fromString(root["sta_ip"].as<String>());
        g_wifi_cfg->sta.Gw.fromString(root["sta_gw"].as<String>());
        g_wifi_cfg->sta.Sn.fromString(root["sta_sm"].as<String>());
        g_wifi_cfg->sta.Dns.fromString(root["sta_dns"].as<String>());
        g_wifi_cfg->sta.Dhcp  = root["sta_dhcp"].as<int>();
        g_wifi_cfg->UDPPort   = root["udp_port"].as<int>();
        g_wifi_cfg->TCPPort   = root["tcp_port"].as<int>();
        g_wifi_cfg->WSPort    = root["ws_port"].as<int>(); 
        g_wifi_cfg->sta.Dis   = !root["sta_on"].as<int>();

        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable();
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}
void ap_setting_post(AsyncWebServerRequest *request) 
{
    wifi_file_json_t *g_wifi_cfg;    
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_PRINTF("JSON parsing failed!");
        return;
    }

    g_wifi_cfg = wifi_info_get();

    if("1234" == root["access_code"])
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");

        root["ap_ssid"].as<String>().toCharArray(g_wifi_cfg->ap.ssid, Df_LengSsid + 1);
        root["ap_pass"].as<String>().toCharArray(g_wifi_cfg->ap.pass, Df_LengPass + 1);
        g_wifi_cfg->ap.Dis    = !root["ap_on"].as<int>();
        g_wifi_cfg->ap.Chanel = root["ap_channel"].as<int>();
        g_wifi_cfg->ap.Hidden = root["ap_hidden"].as<int>(); 
        
        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable();
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void device_address_post(AsyncWebServerRequest *request)
{
    wifi_file_json_t *g_wifi_cfg;    
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_PRINTF("JSON parsing failed!");
        return;
    }

    g_wifi_cfg = wifi_info_get();

    if("1234" == root["access_code"])
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");

        root["device_name"].as<String>().toCharArray(g_wifi_cfg->addr.device_name, Df_LengDevName + 1);
        root["device_addr"].as<String>().toCharArray(g_wifi_cfg->addr.device_addr, Df_LengAddr + 1);        

        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable();
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void auth_access_post(AsyncWebServerRequest *request)
{
    wifi_file_json_t *g_wifi_cfg;   
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_PRINTF("JSON parsing failed!");
        return;
    }

    g_wifi_cfg = wifi_info_get();

    if("1234" == root["access_code"])
    {
        if (root["old_pass"] == g_wifi_cfg->auth.pass)
        {
            request->send(200, "text/json", "Wifi Advance Setting Succeed");

            root["new_pass"].as<String>().toCharArray(g_wifi_cfg->auth.pass, Df_LengAuth + 1);        

            wifi_info_write(g_wifi_cfg);

            /* Reset to access new network */
            esp_reset_enable();
        }
        else
        {
            request->send(200, "text/json", "Old pass Wrong");
        }
        
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void time_setting_post(AsyncWebServerRequest *request)
{    
    AsyncWebParameter* p = request->getParam(0);
    const char *rtc_str = p->value().c_str();
    if (rtc_parse_utility((char*)rtc_str))
    {
        request->send(200, "text/json", "Time Setting Succeed");
    }
    else
    {
        request->send(200, "text/json", "Time Setting Wrong");
    }    
}

void print_handlerequest(AsyncWebServerRequest *request, String &message)
{
  message = "";
  message += "\r\nURI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (HTTP_GET == request->method()) ? "GET" : "POST";
  message += "\nArguments: ";
  message += request->params();
  message += "\n";
  for (uint8_t i = 0; i < request->params(); i++)
  {
    AsyncWebParameter* p = request->getParam(i);
    message += " NAME:" + p->name() + "\n VALUE:" + p->value() + "\n";
  }
  WEB_SERVER_DBG_PORT.println(message);
}
