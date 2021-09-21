#include <ArduinoJson.h>
#include "hth_esp_config.h"
#include "hth_esp_wifi.h"
#include "hth_AsyncEasyDDNS.h"
#include "THIoT_ESPSoftReset.h"
#include "hth_esp_sys_rtc.h"
#include "hth_esp_sys_params.h"
#include "hth_esp_eeprom.h"
#include "hth_esp_sdcard.h"
#include "hth_fs_handle.h"
#include "hth_esp_ethernet.h"
#include "hth_serial_trace.h"
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
void sta_ap_info_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void sta_network_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void sta_setting_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void ap_setting_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void device_info_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void time_setting_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void fw_version_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void restart_device_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void heap_temperature_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void activated_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void format_sd_card_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void ddns_client_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void pass_common_get(AsyncWebServerRequest *request, WebserverURLHandle* client);
void format_spiffs_get(AsyncWebServerRequest *request, WebserverURLHandle* client);

/* Post */
void sta_ap_info_post(AsyncWebServerRequest *request, WebserverURLHandle* client);
void sta_network_post(AsyncWebServerRequest *request, WebserverURLHandle* client);
void sta_setting_post(AsyncWebServerRequest *request, WebserverURLHandle* client);
void ap_setting_post(AsyncWebServerRequest *request, WebserverURLHandle* client);
void device_info_post(AsyncWebServerRequest *request, WebserverURLHandle* client);
void auth_access_post(AsyncWebServerRequest *request, WebserverURLHandle* client);
void time_setting_post(AsyncWebServerRequest *request, WebserverURLHandle* client);
void ddns_client_post(AsyncWebServerRequest *request, WebserverURLHandle* client);
void auth_user_access_post(AsyncWebServerRequest *request, WebserverURLHandle* client);
void pass_common_post(AsyncWebServerRequest *request, WebserverURLHandle* client);

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

WebserverURLHandle::WebserverURLHandle(/* args */){}
WebserverURLHandle::~WebserverURLHandle(){}

/**
 * Handler called after once request with method GET.
 */
void WebserverURLHandle::onHttpGet(AsyncWebServerRequest* request)
{
    onHttpGetAuth(request);
}

/**
 * Handler called after once request with method GET and authenticated.
 */
void WebserverURLHandle::onHttpGetAuth(AsyncWebServerRequest* request)
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
void WebserverURLHandle::onHttpPostAuth(AsyncWebServerRequest* request)
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
void sta_ap_info_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    IPAddress local_ip(0,0,0,0);
    String json_network;
    bool connect_st = false;

    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();
    root["ap_ssid"].set(ESPConfig.ssidAP());
    root["ap_ip_adress"].set(ESPConfig.ipAP().toString());
    root["sta_ssid"].set(ESPConfig.ssidSTA());   
    root["sta_hostname"].set(ESPConfig.hostNameSTA()); 
    root["ap_dns_name"].set(ESPConfig.dnsNameAP()); 

    if(!ESPConfig.dhcpSTA()) 
    {
        root["sta_ip_dhcp"].set("Disable");
    }
    else
    {
        root["sta_ip_dhcp"].set("Enable");
    }

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(Ethernet.isEnable())
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
        if(!ESPConfig.dhcpSTA()) 
        {
            root["sta_ip_address"].set(ESPConfig.ipSTA().toString());
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
void sta_network_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    String json_network = "{\"status\":\"ok\",\"mgs\":\"WiFi is scanning ...\"}";
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(Ethernet.isEnable())
    {
        json_network = "{\"status\":\"error\",\"mgs\":\"Ethernet Mode\"}";
    }
#endif        
    request->send(200, "text/json", json_network);

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(!Ethernet.isEnable())
    {
        client->asyncScanNetwork();
    }
#else
    client->asyncScanNetwork();
#endif
}

void sta_setting_get(AsyncWebServerRequest *request, WebserverURLHandle* client) 
{
    IPAddress local_ip(0,0,0,0);
    IPAddress gateway_ip(0,0,0,0);
    IPAddress subnet_ip(0,0,0,0);
    IPAddress dns_ip(0,0,0,0);
    String json_network;
    uint8_t connect_st = 0;

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(Ethernet.isEnable())
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
    root["sta_ssid"].set(ESPConfig.ssidSTA());
    root["sta_pass"].set(ESPConfig.passSTA()); 
    root["sta_hostname"].set(ESPConfig.hostNameSTA()); 
    if (connect_st)
    {
        root["sta_ip"].set(local_ip.toString());
        root["sta_gw"].set(gateway_ip.toString());
        root["sta_sm"].set(subnet_ip.toString());
        root["sta_dns"].set(dns_ip.toString());   
    }
    else
    {
        if(!ESPConfig.dhcpSTA())
        {
            root["sta_ip"].set(ESPConfig.ipSTA().toString());
            root["sta_gw"].set(ESPConfig.gwSTA().toString());
            root["sta_sm"].set(ESPConfig.snSTA().toString());
            root["sta_dns"].set(ESPConfig.dnsSTA().toString());
        }
        else
        {
            root["sta_ip"].set("0.0.0.0");
            root["sta_gw"].set("0.0.0.0");
            root["sta_sm"].set("0.0.0.0");
            root["sta_dns"].set("0.0.0.0");
        }
        
    }
    
    root["sta_dhcp"].set(ESPConfig.dhcpSTA());
    root["sta_on"].set((ESPConfig.isDisableSTA() == 0) ? 1 : 0);  
    root["udp_port"].set(ESPConfig.udpPort());
    root["tcp_port"].set(ESPConfig.tcpPort());
    root["ws_port"].set(ESPConfig.wsPort());

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void ap_setting_get(AsyncWebServerRequest *request, WebserverURLHandle* client) 
{
    String json_network;
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["ap_ssid"].set(ESPConfig.ssidAP());
    root["ap_pass"].set(ESPConfig.passAP());
    root["ap_dns_name"].set(ESPConfig.dnsNameAP()); 
    root["ap_on"].set((ESPConfig.isDisableAP() == 0) ? 1 : 0);  
    root["ap_channel"].set(ESPConfig.channelAP());
    root["ap_hidden"].set(ESPConfig.isHiddenAP());

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void device_info_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    String json_network;
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["name"].set(ESPConfig.nameDevice());
    root["addr"].set(ESPConfig.addrDevice());

    root.prettyPrintTo(json_network);
    request->send(200, "text/json", json_network);
}

void time_setting_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{    
    String json_network;
    rtc_time_t rtc;

    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();  

    if (ESPTime.get(&rtc))
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
void fw_version_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    char buf[100];
    const char * fwBuild = __DATE__ " " __TIME__ " GMT";
    snprintf(buf, 100, "{\"fw_version\":\"%u.%u.%u\",\"buildTime\":\"%s\"}",
            FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_BUILD, fwBuild);
    request->send(200, "text/json", buf);
}

/* /get?param_wifi=restart */
void restart_device_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    request->send(200, "text/json", "Reset OK");
    HTH_softReset.enable(100);
}

/* /get?param_wifi=heap_temperature */
void heap_temperature_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
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
void activated_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{    
    if (request->argName(1) == "cmd")
    {
        uint8_t cmd = atoi(request->arg(1).c_str());
        if (0 == cmd || 1 == cmd)
        {
            if (1 == cmd)
            {
                EEPParams.deviceActive();
            }
                
            if (0 == cmd)
            {
                EEPParams.deviceInactive();
            }
        }
        request->send(200, "text/html", "Vaule: " + String(EEPParams.isDeviceActivated()));
    }    
}

void format_sd_card_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    if (request->argName(1) == "pass" && ESPConfig.passConfirmIsOK(request->arg(1), ESPSysParams::CONFIRM_COMMON))
    {
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)    
        // if these takes more time, we should make an event format bind to run in loop
#ifdef ESP32
        FSHandle.format(SD_FS_SYSTEM, "/");
#elif defined(ESP8266)
        FSHandle.format(SD_FS_SYSTEM, "/");
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

void format_spiffs_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    if (request->argName(1) == "pass" && ESPConfig.passConfirmIsOK(request->arg(1), ESPSysParams::CONFIRM_COMMON))
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

void ddns_client_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    String json_resp;
    
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["service"].set(ESPConfig.serviceDDNS());
    root["domain"].set(ESPConfig.domainDDNS());
    root["user"].set(ESPConfig.userDDNS());  
    root["pass"].set(ESPConfig.passDDNS());
    root["sync_time"].set(ESPConfig.syncTimeDDNS());
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)    
    root["ip_ddns"].set(ESPWifi.ddnsClient->ddnsIP.toString());
#else
    root["ip_ddns"].set("Disable");
#endif
    root["disable"].set(ESPConfig.disableDDNS());

    root.prettyPrintTo(json_resp);
    request->send(200, "text/json", json_resp);
}

void pass_common_get(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    if (request->argName(1) != "Pass" 
    || !ESPConfig.passConfirmIsOK(request->arg(1), ESPSysParams::CONFIRM_COMMON))
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
void sta_ap_info_post(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
}

/* Post json sta_network 
{
"sta_ssid": "Quyen_2.4G",
"sta_ssid": "12345679",
"access_code": "1234"
}
*/
void sta_network_post(AsyncWebServerRequest *request, WebserverURLHandle* client) 
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Setting Succeed");

        ESPConfig.ssidSTASet(root["sta_ssid"].as<String>());
        ESPConfig.passSTASet(root["sta_pass"].as<String>());
        HTTPSERVER_URL_TAG_CONSOLE("SSID: %s", ESPConfig.ssidSTA().c_str());
        HTTPSERVER_URL_TAG_CONSOLE("PASS: %s", ESPConfig.passSTA().c_str());        

        ESPConfig.save();

        /* Reset to access new network */
        HTH_softReset.enable(500);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void sta_setting_post(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    AsyncWebParameter* p = request->getParam(0);  

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");

        ESPConfig.ssidSTASet(root["sta_ssid"].as<String>());
        ESPConfig.passSTASet(root["sta_pass"].as<String>());
        ESPConfig.hostNameSTASet(root["sta_hostname"].as<String>());
        ESPConfig.ipSTASet(root["sta_ip"].as<String>());
        ESPConfig.gwSTASet(root["sta_gw"].as<String>());
        ESPConfig.snSTASet(root["sta_sn"].as<String>());
        ESPConfig.dnsSTASet(root["sta_dns"].as<String>());
        ESPConfig.dhcpSTASet(root["sta_dhcp"].as<int>());
        ESPConfig.udpPortSet(root["udp_port"].as<int>());
        ESPConfig.tcpPortSet(root["tcp_port"].as<int>());
        ESPConfig.wsPortSet(root["ws_port"].as<int>()); 
        ESPConfig.disableSTASet(!root["sta_on"].as<int>());

        ESPConfig.save();

        /* Reset to access new network */
        HTH_softReset.enable(500);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void ap_setting_post(AsyncWebServerRequest *request, WebserverURLHandle* client) 
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");

        ESPConfig.ssidAPSet(root["ap_ssid"].as<String>());
        ESPConfig.passAPSet(root["ap_pass"].as<String>());
        ESPConfig.dnsNameAPSet(root["ap_dns_name"].as<String>());
        ESPConfig.disableAPSet(!root["ap_on"].as<int>());
        ESPConfig.channelAPSet(root["ap_channel"].as<int>());
        ESPConfig.hiddenAPSet(root["ap_hidden"].as<int>()); 
        
        ESPConfig.save();

        /* Reset to access new network */
        HTH_softReset.enable(500);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void device_info_post(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");
        ESPConfig.nameDeviceSet(root["name"].as<String>());
        ESPConfig.nameDeviceSet(root["addr"].as<String>());      

        ESPConfig.save();

        /* Reset to access new network */
        HTH_softReset.enable(500);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void auth_access_post(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        if (root["old_pass"] == ESPConfig.authAdminPass())
        {
            request->send(200, "text/json", "Setting Password Succeed");

            ESPConfig.authAdminPassSet(root["new_pass"].as<String>());

            ESPConfig.save();

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

void time_setting_post(AsyncWebServerRequest *request, WebserverURLHandle* client)
{    
    AsyncWebParameter* p = request->getParam(0);
    const char *rtc_str = p->value().c_str();
    if (ESPTime.GMTStringUpdate(rtc_str, ESPTimeSystem::RTC_WEB_UPDATE))
    {
        request->send(200, "text/json", "Time Setting Succeed");
    }
    else
    {
        request->send(200, "text/json", "Time Setting Wrong");
    }   
}

void ddns_client_post(AsyncWebServerRequest *request, WebserverURLHandle* client) 
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "DDNS Setting Succeed");

        ESPConfig.serviceDDNSSet(root["service"].as<String>());
        ESPConfig.domainDDNSSet(root["domain"].as<String>());
        ESPConfig.userDDNSSet(root["user"].as<String>());
        ESPConfig.passDDNSSet(root["pass"].as<String>());
        ESPConfig.syncTimeDDNSSet(root["sync_time"].as<int>());  
        ESPConfig.disableDDNSSet(root["disable"].as<int>());
        ESPConfig.save();
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)        
        ESPWifi.ddnsClient->service(ESPConfig.serviceDDNS());
        ESPWifi.ddnsClient->begin(ESPConfig.domainDDNS(), ESPConfig.userDDNS(), ESPConfig.passDDNS());
#endif 
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void auth_user_access_post(AsyncWebServerRequest *request, WebserverURLHandle* client)
{ 
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        if (root["old_pass"] == ESPConfig.authUserPass())
        {
            request->send(200, "text/json", "Setting Password Succeed");

            ESPConfig.authUserPassSet(root["new_pass"].as<String>());

            ESPConfig.save();

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

void pass_common_post(AsyncWebServerRequest *request, WebserverURLHandle* client)
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTPSERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["old_pass"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Setting Password Succeed");   
        ESPConfig.passConfirmSet(ESPSysParams::CONFIRM_COMMON, root["new_pass"].as<String>().toInt());
        ESPConfig.save();
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
