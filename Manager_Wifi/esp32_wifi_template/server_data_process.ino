#include <ArduinoJson.h>
#include "wifi_data_file.h"
#include "server_data_process.h"

#define SERVER_DATA_PORT Serial
#define SERVER_DATA_PRINTF(f_, ...) SERVER_DATA_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void server_data_get_process(void)
{
    for (uint8_t i = 0; i < DATA_GET_HANDLE_NUM; ++i)
    {
        if (server.argName(0) == client_get_handle[i].path_arg)
        {
            SERVER_DATA_PRINTF("\r\nProcess get callback [%u]", i);
            client_get_handle[i].cb();
            break;
        }
    }
}

void server_data_post_process(void)
{
    for (uint8_t i = 0; i < DATA_POST_HANDLE_NUM; ++i)
    {
        if (server.argName(0) == client_post_handle[i].path_arg)
        {
            SERVER_DATA_PRINTF("\r\nProcess post callback [%u]", i);
            client_post_handle[i].cb();
            break;
        }
    }
}

void ata_ap_info_get(void)
{
}

/* Get json sta_network */
void sta_network_get(void)
{
    String json_network = "{\"status\":\"error\",\"mgs\":\"No network\"}";
    esp_ssid_scan(json_network);
    server.send(200, "text/json", json_network);
}
void sta_setting_get(void) {}
void ap_setting_get(void) {}

/* Post json sta_network 
{
"STASsid": "Quyen_2.4G",
"STAPass": "12345679",
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
        root["STASsid"].as<String>().toCharArray(g_wifi_cfg->sta.ssid, Df_LengSsid + 1);
        root["STAPass"].as<String>().toCharArray(g_wifi_cfg->sta.pass, Df_LengPass + 1);
        WEB_SERVER_PRINTF("\r\nSSID: %s", g_wifi_cfg->sta.ssid);
        WEB_SERVER_PRINTF("\r\nPASS: %s\r\n", g_wifi_cfg->sta.pass);

        server.send(200, "text/json", "Network Setting succeed");

        //Truong hop ssid length = 0 nghia la khong ket noi den wlan
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

void sta_setting_post(void) {}
void ap_setting_post(void) {}
