#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "app_config.h"
#include "eeprom_data.h"
#include "rtc_data_file.h"
#include "wifi_data_file.h"
#include "server_data_process.h"
#include "AsyncEasyDDNS.h"

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
    IPAddress local_ip(0,0,0,0);
    String json_network;
    wifi_file_json_t *g_wifi_cfg;
    uint8_t connect_st = 0;
    g_wifi_cfg = wifi_info_get();

    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();
    root["ap_ssid"].set(g_wifi_cfg->ap.ssid);
    root["ap_ip_adress"].set(g_wifi_cfg->ap.ip.toString());
    root["sta_ssid"].set(g_wifi_cfg->sta.ssid);   

    if(!g_wifi_cfg->sta.dhcp) 
    {
        root["sta_ip_dhcp"].set("Disable");
    }
    else
    {
        root["sta_ip_dhcp"].set("Enable");
    }

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(eth_is_enable())
    {
        if(ETH.linkUp())
        {
            SERVER_DATA_PRINTF("\r\nETH.linkUp OK");
            connect_st = 1;
            local_ip = ETH.localIP();
        }
    }
    else
    {
        if (WL_CONNECTED == WiFi.status())
        {
            connect_st = 1;
            local_ip = WiFi.localIP();
        }
    }
#else
    if (WL_CONNECTED == WiFi.status())
    {
        connect_st = 1;
        local_ip = WiFi.localIP();
    }
#endif
    
    
    if (connect_st)
    {
        root["sta_ip_address"].set(local_ip.toString());
        root["sta_status"].set("Connected");
    }
    else
    {
        if(!g_wifi_cfg->sta.dhcp) 
        {
            root["sta_ip_address"].set(g_wifi_cfg->sta.ip.toString());
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
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(!eth_is_enable())
    {
        esp_ssid_scan(json_network);
    }
#else
    esp_ssid_scan(json_network);
#endif        
    request->send(200, "text/json", json_network);
}

void sta_setting_get(AsyncWebServerRequest *request) 
{
    IPAddress local_ip(0,0,0,0);
    IPAddress gateway_ip(0,0,0,0);
    IPAddress subnet_ip(0,0,0,0);
    IPAddress dns_ip(0,0,0,0);
    String json_network;
    wifi_file_json_t *g_wifi_cfg;
    uint8_t connect_st = 0;
    g_wifi_cfg = wifi_info_get();

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(eth_is_enable())
    {
        if(ETH.linkUp())
        {
            SERVER_DATA_PRINTF("\r\nETH.linkUp OK");
            connect_st = 1;
            local_ip = ETH.localIP();
            gateway_ip = ETH.gatewayIP();
            subnet_ip = ETH.subnetMask();
            dns_ip = ETH.dnsIP();
        }
    }
    else
    {
        if (WL_CONNECTED == WiFi.status())
        {
            connect_st = 1;
            local_ip = WiFi.localIP();
            gateway_ip = WiFi.gatewayIP();
            subnet_ip = WiFi.subnetMask();
            dns_ip = WiFi.dnsIP();
        }
    }
#else
    if (WL_CONNECTED == WiFi.status())
    {
        connect_st = 1;
        local_ip = WiFi.localIP();
        gateway_ip = WiFi.gatewayIP();
        subnet_ip = WiFi.subnetMask();
        dns_ip = WiFi.dnsIP();
    }
#endif
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["sta_ssid"].set(g_wifi_cfg->sta.ssid);
    root["sta_pass"].set(g_wifi_cfg->sta.pass); 
    if (connect_st)
    {
        root["sta_ip"].set(local_ip.toString());
        root["sta_gw"].set(gateway_ip.toString());
        root["sta_sm"].set(subnet_ip.toString());
        root["sta_dns"].set(dns_ip.toString());   
    }
    else
    {
        if(!g_wifi_cfg->sta.dhcp)
        {
            root["sta_ip"].set(g_wifi_cfg->sta.ip.toString());
            root["sta_gw"].set(g_wifi_cfg->sta.gw.toString());
            root["sta_sm"].set(g_wifi_cfg->sta.sn.toString());
            root["sta_dns"].set(g_wifi_cfg->sta.dns.toString());
        }
        else
        {
            root["sta_ip"].set("0.0.0.0");
            root["sta_gw"].set("0.0.0.0");
            root["sta_sm"].set("0.0.0.0");
            root["sta_dns"].set("0.0.0.0");
        }
        
    }
    
    root["sta_dhcp"].set(g_wifi_cfg->sta.dhcp);
    root["sta_on"].set(!g_wifi_cfg->sta.disable);  
    root["udp_port"].set(g_wifi_cfg->port.udp);
    root["tcp_port"].set(g_wifi_cfg->port.tcp);
    root["ws_port"].set(g_wifi_cfg->port.ws);

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
    root["ap_on"].set(!g_wifi_cfg->ap.disable);  
    root["ap_channel"].set(g_wifi_cfg->ap.channel);
    root["ap_hidden"].set(g_wifi_cfg->ap.hidden);

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void device_info_get(AsyncWebServerRequest *request)
{
    String json_network;
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["name"].set(g_wifi_cfg->addr.name);
    root["addr"].set(g_wifi_cfg->addr.addr);

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
    char buf[100];
    const char * buildTime = __DATE__ " " __TIME__ " GMT";
    snprintf(buf, 100, "{\"fw_version\":\"%u.%u.%u\",\"buildTime\":\"%s\"}",
            FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_BUILD, buildTime);
    request->send(200, "text/json", buf);
}

/* /get?param_wifi=restart */
void restart_device_get(AsyncWebServerRequest *request)
{
    request->send(200, "text/json", "Reset OK");
    log_report(LOG_REPORT_RESET, (char *)"Web reset3");
    esp_reset_enable(500);
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

void ddns_client_get(AsyncWebServerRequest *request)
{
    String json_resp;
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["service"].set(g_wifi_cfg->ddns.service);
    root["domain"].set(g_wifi_cfg->ddns.domain);
    root["user"].set(g_wifi_cfg->ddns.user);  
    root["pass"].set(g_wifi_cfg->ddns.pass);
    root["sync_time"].set(g_wifi_cfg->ddns.sync_time);
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)    
    root["ip_ddns"].set(AsyncEasyDDNS.ddnsip.toString());
#else
    root["ip_ddns"].set("Disable");
#endif
    root["disable"].set(g_wifi_cfg->ddns.disable);

    root.prettyPrintTo(json_resp);
    request->send(200, "text/json", json_resp);
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
        esp_reset_enable(500);
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
        g_wifi_cfg->sta.ip.fromString(root["sta_ip"].as<String>());
        g_wifi_cfg->sta.gw.fromString(root["sta_gw"].as<String>());
        g_wifi_cfg->sta.sn.fromString(root["sta_sm"].as<String>());
        g_wifi_cfg->sta.dns.fromString(root["sta_dns"].as<String>());
        g_wifi_cfg->sta.dhcp  = root["sta_dhcp"].as<int>();
        g_wifi_cfg->port.udp   = root["udp_port"].as<int>();
        g_wifi_cfg->port.tcp   = root["tcp_port"].as<int>();
        g_wifi_cfg->port.ws    = root["ws_port"].as<int>(); 
        g_wifi_cfg->sta.disable   = !root["sta_on"].as<int>();

        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable(500);
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
        g_wifi_cfg->ap.disable    = !root["ap_on"].as<int>();
        g_wifi_cfg->ap.channel = root["ap_channel"].as<int>();
        g_wifi_cfg->ap.hidden = root["ap_hidden"].as<int>(); 
        
        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable(500);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void device_info_post(AsyncWebServerRequest *request)
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

        root["name"].as<String>().toCharArray(g_wifi_cfg->addr.name, Df_LengDevName + 1);
        root["addr"].as<String>().toCharArray(g_wifi_cfg->addr.addr, Df_LengAddr + 1);        

        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable(500);
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
            esp_reset_enable(500);
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

void ddns_client_post(AsyncWebServerRequest *request) 
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
        request->send(200, "text/json", "DDNS Setting Succeed");

        root["service"].as<String>().toCharArray(g_wifi_cfg->ddns.service, DDNS_SERVICE_LENGTH_MAX + 1);
        root["domain"].as<String>().toCharArray(g_wifi_cfg->ddns.domain, DDNS_DOMAIN_LENGTH_MAX + 1);
        root["user"].as<String>().toCharArray(g_wifi_cfg->ddns.user, DDNS_USER_LENGTH_MAX + 1);
        root["pass"].as<String>().toCharArray(g_wifi_cfg->ddns.pass, DDNS_PASS_LENGTH_MAX + 1);
        g_wifi_cfg->ddns.sync_time = root["sync_time"].as<int>();  
        g_wifi_cfg->ddns.disable = root["disable"].as<int>();  
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)        
        AsyncEasyDDNS.service(g_wifi_cfg->ddns.service);
        AsyncEasyDDNS.client(g_wifi_cfg->ddns.domain, g_wifi_cfg->ddns.user, g_wifi_cfg->ddns.pass);
#endif        
        wifi_info_write(g_wifi_cfg);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
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
