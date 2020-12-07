#include <ArduinoJson.h>
#include "wifi_data_file.h"
#include "server_data_process.h"

#define SERVER_DATA_PORT Serial
#define SERVER_DATA_PRINTF(f_, ...) SERVER_DATA_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void server_data_get_process(void)
{
    /* param wifi get 
     * /get?argName=arg(0)
     * using argName(0) = "param_wifi" is parameter to get wifi information 
     * using arg(0) is parameter to find callback_post
     */
    if (server.argName(0) == "param_wifi")
    {
        for (uint8_t i = 0; i < DATA_GET_HANDLE_NUM; ++i)
        {
            if (server.arg(0) == client_get_handle[i].path_arg)
            {
                SERVER_DATA_PRINTF("\r\nProcess get callback [%u]", i);
                client_get_handle[i].cb();
                break;
            }
        }
    }
    else
    {
        String arg_str = "";
        print_handlerequest(arg_str);
        server.send(200, "text/html", arg_str);
    }
    
}

void server_data_post_process(void)
{
    uint8_t cb = 0;
    /* param wifi post 
     * /post?
     * argName(0) = arg(0)
     * using argName(0) is parameter to find callback_post
     * using arg(0) is data
     */
    for (uint8_t i = 0; i < DATA_POST_HANDLE_NUM; ++i)
    {
        if (server.argName(0) == client_post_handle[i].path_arg)
        {
            SERVER_DATA_PRINTF("\r\nProcess post callback [%u]", i);
            client_post_handle[i].cb();
            cb = 1;
            break;
        }
    }

    /* non process callback, so return arg parse */
    if (!cb)
    {
        String arg_str = "";
        print_handlerequest(arg_str);
        server.send(200, "text/html", arg_str);
    }
}

/*---------------------------------------------------------------------*
 *----------------------------data get process-------------------------*
 *---------------------------------------------------------------------*/
void sta_ap_info_get(void)
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
    server.send(200, "text/json", json_network);
}

/* Get json sta_network */
void sta_network_get(void)
{
    String json_network = "{\"status\":\"error\",\"mgs\":\"No network\"}";
    esp_ssid_scan(json_network);
    server.send(200, "text/json", json_network);
}
void sta_setting_get(void) 
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
    server.send(200, "text/json", json_network);
}

void ap_setting_get(void) 
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
    server.send(200, "text/json", json_network);
}

/*---------------------------------------------------------------------*
 *----------------------------data post process------------------------*
 *---------------------------------------------------------------------*/
void sta_ap_info_post(void)
{
}

/* Post json sta_network 
{
"sta_ssid": "Quyen_2.4G",
"sta_ssid": "12345679",
"access_code": "1234"
}
*/
void sta_network_post(void) 
{
    wifi_file_json_t *g_wifi_cfg;

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(server.arg(0));
    if (!root.success())
    {
        SERVER_DATA_PRINTF("JSON parsing failed!");
        return;
    }

    g_wifi_cfg = wifi_info_get();

    if("1234" == root["access_code"])
    {
        server.send(200, "text/json", "Wifi Setting Succeed");

        root["sta_ssid"].as<String>().toCharArray(g_wifi_cfg->sta.ssid, Df_LengSsid + 1);
        root["sta_pass"].as<String>().toCharArray(g_wifi_cfg->sta.pass, Df_LengPass + 1);
        WEB_SERVER_PRINTF("\r\nSSID: %s", g_wifi_cfg->sta.ssid);
        WEB_SERVER_PRINTF("\r\nPASS: %s\r\n", g_wifi_cfg->sta.pass);        

        //Cập nhật thông số vào FS
        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable();
    }
    else
    {
        server.send(200, "text/json", "Password Setting Wrong");
    }
}

void sta_setting_post(void)
{
    wifi_file_json_t *g_wifi_cfg;    

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(server.arg(0));
    if (!root.success())
    {
        SERVER_DATA_PRINTF("JSON parsing failed!");
        return;
    }

    g_wifi_cfg = wifi_info_get();

    if("1234" == root["access_code"])
    {
        server.send(200, "text/json", "Wifi Advance Setting Succeed");

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

        //Cập nhật thông số vào FS
        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable();
    }
    else
    {
        server.send(200, "text/json", "Password Setting Wrong");
    }
}
void ap_setting_post(void) 
{
    wifi_file_json_t *g_wifi_cfg;    

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(server.arg(0));
    if (!root.success())
    {
        SERVER_DATA_PRINTF("JSON parsing failed!");
        return;
    }

    g_wifi_cfg = wifi_info_get();

    if("1234" == root["access_code"])
    {
        server.send(200, "text/json", "Wifi Advance Setting Succeed");

        root["ap_ssid"].as<String>().toCharArray(g_wifi_cfg->ap.ssid, Df_LengSsid + 1);
        root["ap_pass"].as<String>().toCharArray(g_wifi_cfg->ap.pass, Df_LengPass + 1);
        g_wifi_cfg->ap.Dis    = !root["ap_on"].as<int>();
        g_wifi_cfg->ap.Chanel = root["ap_channel"].as<int>();
        g_wifi_cfg->ap.Hidden = root["ap_hidden"].as<int>(); 
        

        //Cập nhật thông số vào FS
        wifi_info_write(g_wifi_cfg);

        /* Reset to access new network */
        esp_reset_enable();
    }
    else
    {
        server.send(200, "text/json", "Password Setting Wrong");
    }
}
