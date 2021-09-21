#include <ArduinoJson.h>
#include "hth_esp_config.h"
#include "hth_esp_wifi.h"
#include "hth_AsyncEasyDDNS.h"
#include "hth_esp_soft_reset.h"
#include "hth_esp_sys_rtc.h"
#include "hth_esp_sys_params.h"
#include "hth_esp_eeprom.h"
#include "hth_esp_sdcard.h"
#include "hth_fs_handle.h"
#include "hth_esp_ethernet.h"
#include "hth_console_dbg.h"
#include "hth_httpserver_url.h"

#define SERVER_DATA_PORT CONSOLE_PORT
#define SERVER_DATA_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define HTTPSERVER_URL_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[HTTP_SERVER]", __VA_ARGS__)

#define DATA_GET_HANDLE_NUM     14
#define DATA_POST_HANDLE_NUM    10

#ifdef ESP32
extern "C" {
    uint8_t temprature_sens_read(void);
}

float esp_internal_temp(void)
{
    float temp;
    temp = (temprature_sens_read() - 32) / 1.8;
    HTTPSERVER_URL_TAG_CONSOLE("Temp: %.2f", temp);
    return temp;
}
#elif defined(ESP8266)
float esp_internal_temp(void)
{
    return 25.00;
}
#endif

void print_handlerequest(AsyncWebServerRequest *request, String &message);

/* get */
void sta_ap_info_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void sta_network_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void sta_setting_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void ap_setting_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void device_info_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void time_setting_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void fw_version_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void restart_device_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void heap_temperature_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void activated_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void format_sd_card_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void ddns_client_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void pass_common_get(AsyncWebServerRequest *request, hth_httpserver_url* client);
void format_spiffs_get(AsyncWebServerRequest *request, hth_httpserver_url* client);

/* Post */
void sta_ap_info_post(AsyncWebServerRequest *request, hth_httpserver_url* client);
void sta_network_post(AsyncWebServerRequest *request, hth_httpserver_url* client);
void sta_setting_post(AsyncWebServerRequest *request, hth_httpserver_url* client);
void ap_setting_post(AsyncWebServerRequest *request, hth_httpserver_url* client);
void device_info_post(AsyncWebServerRequest *request, hth_httpserver_url* client);
void auth_access_post(AsyncWebServerRequest *request, hth_httpserver_url* client);
void time_setting_post(AsyncWebServerRequest *request, hth_httpserver_url* client);
void ddns_client_post(AsyncWebServerRequest *request, hth_httpserver_url* client);
void auth_user_access_post(AsyncWebServerRequest *request, hth_httpserver_url* client);
void pass_common_post(AsyncWebServerRequest *request, hth_httpserver_url* client);

/* /get?param_wifi=[param] */
server_get_handle_t client_get_handle[DATA_GET_HANDLE_NUM] = {
/*00*/{(char*)"sta_ap_info", sta_ap_info_get},
/*01*/{(char*)"sta_network", sta_network_get},
/*02*/{(char*)"sta_setting", sta_setting_get},
/*03*/{(char*)"ap_setting", ap_setting_get},
/*04*/{(char*)"device_info", device_info_get},
/*05*/{(char*)"time_setting", time_setting_get},
/*06*/{(char*)"fw_version", fw_version_get},
/*07*/{(char*)"restart", restart_device_get},
/*08*/{(char*)"heap_temperature", heap_temperature_get},
/*09*/{(char*)"activated", activated_get},
/*10*/{(char*)"format_sd_card", format_sd_card_get},
/*11*/{(char*)"ddns_client", ddns_client_get},
/*12*/{(char*)"pass_common", pass_common_get},
/*13*/{(char*)"format_spiffs", format_spiffs_get}
};

server_post_handle_t client_post_handle[DATA_POST_HANDLE_NUM] = {
/*00*/{(char*)"sta_ap_info", sta_ap_info_post},
/*01*/{(char*)"sta_network", sta_network_post},
/*02*/{(char*)"sta_setting", sta_setting_post},
/*03*/{(char*)"ap_setting", ap_setting_post},
/*04*/{(char*)"device_info", device_info_post},
/*05*/{(char*)"auth_access", auth_access_post},
/*06*/{(char*)"time_setting", time_setting_post},
/*07*/{(char*)"ddns_client", ddns_client_post},
/*08*/{(char*)"auth_user_access", auth_user_access_post},
/*09*/{(char*)"pass_common", pass_common_post}
};

hth_httpserver_url::hth_httpserver_url(/* args */){}
hth_httpserver_url::~hth_httpserver_url(){}

/**
 * Handler called after once request with method GET.
 */
void hth_httpserver_url::onHttpGet(AsyncWebServerRequest* request)
{
    onHttpGetAuth(request);
}

/**
 * Handler called after once request with method GET and authenticated.
 */
void hth_httpserver_url::onHttpGetAuth(AsyncWebServerRequest* request)
{
    bool isHandler = false;
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
                HTTPSERVER_URL_TAG_CONSOLE("get [%u]: argName = \"%s\"", i, client_get_handle[i].path_arg);
                client_get_handle[i].cb(request, this);
                isHandler = true;
                break;
            }
        }
    }
    /* non process callback, so return arg parse */
    if (!isHandler)
    {
        String arg_str = "";
        print_handlerequest(request, arg_str);
        request->send(200, "text/html", arg_str);
    }
    
}

/**
 * Handler called after once request with method POST and authenticated.
 */
void hth_httpserver_url::onHttpPostAuth(AsyncWebServerRequest* request)
{
    bool isHandler = false;
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
            HTTPSERVER_URL_TAG_CONSOLE("post [%u]: argName = \"%s\"", i, client_post_handle[i].path_arg);
            client_post_handle[i].cb(request, this);
            isHandler = true;
            break;
        }
    }

    /* non process callback, so return arg parse */
    if (!isHandler)
    {
        String arg_str = "";
        print_handlerequest(request, arg_str);
        request->send(200, "text/html", arg_str);
    }
}

/*---------------------------------------------------------------------*
 *----------------------------data get process-------------------------*
 *---------------------------------------------------------------------*/
void sta_ap_info_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    IPAddress local_ip(0,0,0,0);
    String json_network;
    bool connect_st = false;

    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();
    root["ap_ssid"].set(WFDataFile.ssidAP());
    root["ap_ip_adress"].set(WFDataFile.ipAP().toString());
    root["sta_ssid"].set(WFDataFile.ssidSTA());   
    root["sta_hostname"].set(WFDataFile.hostNameSTA()); 
    root["ap_dns_name"].set(WFDataFile.dnsNameAP()); 

    if(!WFDataFile.dhcpSTA()) 
    {
        root["sta_ip_dhcp"].set("Disable");
    }
    else
    {
        root["sta_ip_dhcp"].set("Enable");
    }

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(HTH_ethernet.isEnable())
    {
#ifdef ESP32
        if(ETH.linkUp())
#elif defined(ESP8266)
        if(ETH.linkUp())
#endif
        {
            HTTPSERVER_URL_TAG_CONSOLE("ETH.linkUp OK");
            connect_st = true;
            local_ip = ETH.localIP();
        }
    }
    else
    {
        if (WL_CONNECTED == WiFi.status())
        {
            connect_st = true;
            local_ip = WiFi.localIP();
        }
    }
#else
    if (WL_CONNECTED == WiFi.status())
    {
        connect_st = true;
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
        if(!WFDataFile.dhcpSTA()) 
        {
            root["sta_ip_address"].set(WFDataFile.ipSTA().toString());
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
void sta_network_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    String json_network = "{\"status\":\"ok\",\"mgs\":\"WiFi is scanning ...\"}";
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(HTH_ethernet.isEnable())
    {
        json_network = "{\"status\":\"error\",\"mgs\":\"Ethernet Mode\"}";
    }
#endif        
    request->send(200, "text/json", json_network);

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(!HTH_ethernet.isEnable())
    {
        client->asyncScanNetwork();
    }
#else
    client->asyncScanNetwork();
#endif
}

void sta_setting_get(AsyncWebServerRequest *request, hth_httpserver_url* client) 
{
    IPAddress local_ip(0,0,0,0);
    IPAddress gateway_ip(0,0,0,0);
    IPAddress subnet_ip(0,0,0,0);
    IPAddress dns_ip(0,0,0,0);
    String json_network;
    uint8_t connect_st = 0;

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(HTH_ethernet.isEnable())
    {
#ifdef ESP32
        if(ETH.linkUp())
#elif defined(ESP8266)
        if(ETH.linkUp())
#endif
        {
            HTTPSERVER_URL_TAG_CONSOLE("ETH.linkUp OK");
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
    root["sta_ssid"].set(WFDataFile.ssidSTA());
    root["sta_pass"].set(WFDataFile.passSTA()); 
    root["sta_hostname"].set(WFDataFile.hostNameSTA()); 
    if (connect_st)
    {
        root["sta_ip"].set(local_ip.toString());
        root["sta_gw"].set(gateway_ip.toString());
        root["sta_sm"].set(subnet_ip.toString());
        root["sta_dns"].set(dns_ip.toString());   
    }
    else
    {
        if(!WFDataFile.dhcpSTA())
        {
            root["sta_ip"].set(WFDataFile.ipSTA().toString());
            root["sta_gw"].set(WFDataFile.gwSTA().toString());
            root["sta_sm"].set(WFDataFile.snSTA().toString());
            root["sta_dns"].set(WFDataFile.dnsSTA().toString());
        }
        else
        {
            root["sta_ip"].set("0.0.0.0");
            root["sta_gw"].set("0.0.0.0");
            root["sta_sm"].set("0.0.0.0");
            root["sta_dns"].set("0.0.0.0");
        }
        
    }
    
    root["sta_dhcp"].set(WFDataFile.dhcpSTA());
    root["sta_on"].set((WFDataFile.isDisableSTA() == 0) ? 1 : 0);  
    root["udp_port"].set(WFDataFile.udpPort());
    root["tcp_port"].set(WFDataFile.tcpPort());
    root["ws_port"].set(WFDataFile.wsPort());

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void ap_setting_get(AsyncWebServerRequest *request, hth_httpserver_url* client) 
{
    String json_network;
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["ap_ssid"].set(WFDataFile.ssidAP());
    root["ap_pass"].set(WFDataFile.passAP());
    root["ap_dns_name"].set(WFDataFile.dnsNameAP()); 
    root["ap_on"].set((WFDataFile.isDisableAP() == 0) ? 1 : 0);  
    root["ap_channel"].set(WFDataFile.channelAP());
    root["ap_hidden"].set(WFDataFile.isHiddenAP());

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void device_info_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    String json_network;
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["name"].set(WFDataFile.nameDevice());
    root["addr"].set(WFDataFile.addrDevice());

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void time_setting_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{    
    String json_network;
    rtc_time_t rtc;

    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();  

    if (HTH_sysTime.getTimeDate(&rtc))
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
void fw_version_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    char buf[100];
    const char * fwBuild = __DATE__ " " __TIME__ " GMT";
    snprintf(buf, 100, "{\"fw_version\":\"%u.%u.%u\",\"buildTime\":\"%s\"}",
            FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_BUILD, fwBuild);
    request->send(200, "text/json", buf);
}

/* /get?param_wifi=restart */
void restart_device_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    request->send(200, "text/json", "Reset OK");
    HTH_softReset.enable(100);
}

/* /get?param_wifi=heap_temperature */
void heap_temperature_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(esp_internal_temp());
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
void activated_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{    
    if (request->argName(1) == "cmd")
    {
        uint8_t cmd = atoi(request->arg(1).c_str());
        if (0 == cmd || 1 == cmd)
        {
            if (1 == cmd)
            {
                HTH_espEEPROM.deviceActive();
            }
                
            if (0 == cmd)
            {
                HTH_espEEPROM.deviceInactive();
            }
        }
        request->send(200, "text/html", "Vaule: " + String(HTH_espEEPROM.isDeviceActivated()));
    }    
}

void format_sd_card_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    if (request->argName(1) == "pass" && WFDataFile.passConfirmIsOK(request->arg(1), hth_esp_sys_params::CONFIRM_COMMON))
    {
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)    
        // if these takes more time, we should make an event format bind to run in loop
#ifdef ESP32
        HTH_fsHandle.format(SD_FS_SYSTEM, "/");
#elif defined(ESP8266)
        HTH_fsHandle.format(SD_FS_SYSTEM, "/");
        // SD_FS_SYSTEM.format(); // failed
#endif
        request->send(200, "text/html", "Format SD card Succeed");
#else
        request->send(200, "text/html", "No Support SD Card");
#endif     
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    } 
}

void format_spiffs_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    if (request->argName(1) == "pass" && WFDataFile.passConfirmIsOK(request->arg(1), hth_esp_sys_params::CONFIRM_COMMON))
    {
        // if these takes more time, we should make an event format bind to run in loop
        NAND_FS_SYSTEM.format();
        request->send(200, "text/html", "Format SPIFFS Succeed");    
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    } 
}

void ddns_client_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    String json_resp;
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["service"].set(WFDataFile.serviceDDNS());
    root["domain"].set(WFDataFile.domainDDNS());
    root["user"].set(WFDataFile.userDDNS());  
    root["pass"].set(WFDataFile.passDDNS());
    root["sync_time"].set(WFDataFile.syncTimeDDNS());
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)    
    root["ip_ddns"].set(HTH_espWifi.ddnsClient->ddnsIP.toString());
#else
    root["ip_ddns"].set("Disable");
#endif
    root["disable"].set(WFDataFile.disableDDNS());

    root.prettyPrintTo(json_resp);
    request->send(200, "text/json", json_resp);
}

void pass_common_get(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    if (request->argName(1) != "Pass" 
    || !WFDataFile.passConfirmIsOK(request->arg(1), hth_esp_sys_params::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "{\"St\":\"Error\"}");
    }
    else
    {
        request->send(200, "text/json", "{\"St\":\"OK\"}");
    }
}

/*---------------------------------------------------------------------*
 *----------------------------data post process------------------------*
 *---------------------------------------------------------------------*/
void sta_ap_info_post(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
}

/* Post json sta_network 
{
"sta_ssid": "Quyen_2.4G",
"sta_ssid": "12345679",
"access_code": "1234"
}
*/
void sta_network_post(AsyncWebServerRequest *request, hth_httpserver_url* client) 
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(WFDataFile.passConfirmIsOK(root["access_code"].as<String>(), hth_esp_sys_params::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Setting Succeed");

        WFDataFile.ssidSTASet(root["sta_ssid"].as<String>());
        WFDataFile.passSTASet(root["sta_pass"].as<String>());
        HTTPSERVER_URL_TAG_CONSOLE("SSID: %s", WFDataFile.ssidSTA().c_str());
        HTTPSERVER_URL_TAG_CONSOLE("PASS: %s", WFDataFile.passSTA().c_str());        

        WFDataFile.commitToFS();

        /* Reset to access new network */
        HTH_softReset.enable(500);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void sta_setting_post(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    AsyncWebParameter* p = request->getParam(0);  

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(WFDataFile.passConfirmIsOK(root["access_code"].as<String>(), hth_esp_sys_params::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");

        WFDataFile.ssidSTASet(root["sta_ssid"].as<String>());
        WFDataFile.passSTASet(root["sta_pass"].as<String>());
        WFDataFile.hostNameSTASet(root["sta_hostname"].as<String>());
        WFDataFile.ipSTASet(root["sta_ip"].as<String>());
        WFDataFile.gwSTASet(root["sta_gw"].as<String>());
        WFDataFile.snSTASet(root["sta_sn"].as<String>());
        WFDataFile.dnsSTASet(root["sta_dns"].as<String>());
        WFDataFile.dhcpSTASet(root["sta_dhcp"].as<int>());
        WFDataFile.udpPortSet(root["udp_port"].as<int>());
        WFDataFile.tcpPortSet(root["tcp_port"].as<int>());
        WFDataFile.wsPortSet(root["ws_port"].as<int>()); 
        WFDataFile.disableSTASet(!root["sta_on"].as<int>());

        WFDataFile.commitToFS();

        /* Reset to access new network */
        HTH_softReset.enable(500);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void ap_setting_post(AsyncWebServerRequest *request, hth_httpserver_url* client) 
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(WFDataFile.passConfirmIsOK(root["access_code"].as<String>(), hth_esp_sys_params::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");

        WFDataFile.ssidAPSet(root["ap_ssid"].as<String>());
        WFDataFile.passAPSet(root["ap_pass"].as<String>());
        WFDataFile.dnsNameAPSet(root["ap_dns_name"].as<String>());
        WFDataFile.disableAPSet(!root["ap_on"].as<int>());
        WFDataFile.channelAPSet(root["ap_channel"].as<int>());
        WFDataFile.hiddenAPSet(root["ap_hidden"].as<int>()); 
        
        WFDataFile.commitToFS();

        /* Reset to access new network */
        HTH_softReset.enable(500);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void device_info_post(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(WFDataFile.passConfirmIsOK(root["access_code"].as<String>(), hth_esp_sys_params::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");
        WFDataFile.nameDeviceSet(root["name"].as<String>());
        WFDataFile.nameDeviceSet(root["addr"].as<String>());      

        WFDataFile.commitToFS();

        /* Reset to access new network */
        HTH_softReset.enable(500);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void auth_access_post(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(WFDataFile.passConfirmIsOK(root["access_code"].as<String>(), hth_esp_sys_params::CONFIRM_COMMON))
    {
        if (root["old_pass"] == WFDataFile.authAdminPass())
        {
            request->send(200, "text/json", "Setting Password Succeed");

            WFDataFile.authAdminPassSet(root["new_pass"].as<String>());

            WFDataFile.commitToFS();

            /* Reset to access new network */
            HTH_softReset.enable(500);
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

void time_setting_post(AsyncWebServerRequest *request, hth_httpserver_url* client)
{    
    AsyncWebParameter* p = request->getParam(0);
    const char *rtc_str = p->value().c_str();
    if (HTH_sysTime.GMTStringUpdate(rtc_str, hth_esp_sys_rtc::RTC_WEB_UPDATE))
    {
        request->send(200, "text/json", "Time Setting Succeed");
    }
    else
    {
        request->send(200, "text/json", "Time Setting Wrong");
    }   
}

void ddns_client_post(AsyncWebServerRequest *request, hth_httpserver_url* client) 
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(WFDataFile.passConfirmIsOK(root["access_code"].as<String>(), hth_esp_sys_params::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "DDNS Setting Succeed");

        WFDataFile.serviceDDNSSet(root["service"].as<String>());
        WFDataFile.domainDDNSSet(root["domain"].as<String>());
        WFDataFile.userDDNSSet(root["user"].as<String>());
        WFDataFile.passDDNSSet(root["pass"].as<String>());
        WFDataFile.syncTimeDDNSSet(root["sync_time"].as<int>());  
        WFDataFile.disableDDNSSet(root["disable"].as<int>());
        WFDataFile.commitToFS();
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)        
        HTH_espWifi.ddnsClient->service(WFDataFile.serviceDDNS());
        HTH_espWifi.ddnsClient->begin(WFDataFile.domainDDNS(), WFDataFile.userDDNS(), WFDataFile.passDDNS());
#endif 
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void auth_user_access_post(AsyncWebServerRequest *request, hth_httpserver_url* client)
{ 
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(WFDataFile.passConfirmIsOK(root["access_code"].as<String>(), hth_esp_sys_params::CONFIRM_COMMON))
    {
        if (root["old_pass"] == WFDataFile.authUserPass())
        {
            request->send(200, "text/json", "Setting Password Succeed");

            WFDataFile.authUserPassSet(root["new_pass"].as<String>());

            WFDataFile.commitToFS();

            /* Reset to access new network */
            HTH_softReset.enable(500);
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

void pass_common_post(AsyncWebServerRequest *request, hth_httpserver_url* client)
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(WFDataFile.passConfirmIsOK(root["old_pass"].as<String>(), hth_esp_sys_params::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Setting Password Succeed");   
        WFDataFile.passConfirmSet(hth_esp_sys_params::CONFIRM_COMMON, root["new_pass"].as<String>().toInt());
        WFDataFile.commitToFS();
    }
    else
    {
        request->send(200, "text/json", "Old pass Wrong");
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
  HTTPSERVER_URL_TAG_CONSOLE("[handlerequest] %s", message.c_str());
}
