#include <WifiType.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "THIoT_APPConfig.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPTimeType.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPWebserverURLHandle.h"
#include "THIoT_ESPAsyncEasyDDNS.h"
#include "THIoT_ESPFSHandle.h"
#include "THIoT_ESPSDFSClass.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_ESPWifiHandle.h"
#include "THIoT_ESPEEPromParams.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFSerialTrace.h"

#define SERVER_DATA_PORT         SERIAL_PORT
#define SERVER_DATA_CONSOLE(...) //SERIAL_LOGI(__VA_ARGS__)
#define HTTP_SERVER_URL_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[HTTP_SERVER]", __VA_ARGS__)

#define SERVER_DATA_TAG_LOG(...) FS_TAG_LOGI("[WEB_DATA]", __VA_ARGS__)
#define SERVER_DATA_FUNCTION_TAG_LOG(...) FS_FUNCTION_TAG_LOGI("[WEB_DATA]", __VA_ARGS__)

#ifdef ESP32
extern "C" float temperatureRead();
static float esp32_internal_temp() {
    float temp;
    temp = temperatureRead();
    SERVER_DATA_CONSOLE("\r\nTemp: %.2f", temp);
    return temp;
}
#elif defined(ESP8266)
float esp_internal_temp(void)
{
    return 25.00;
}
#endif

extern const char *build_time;

/* /get?param_wifi=[param] */
static server_get_handle_t client_get_handle[DATA_GET_HANDLE_NUM] = {
    /*00*/{(const char*)"sta_ap_info", sta_ap_info_get},
    /*01*/{(const char*)"sta_network", sta_network_get},
    /*02*/{(const char*)"sta_setting", sta_setting_get},
    /*03*/{(const char*)"ap_setting", ap_setting_get},
    /*04*/{(const char*)"sntp_setting", sntp_setting_get},
    /*05*/{(const char*)"device_info", device_info_get},
    /*06*/{(const char*)"time_setting", time_setting_get},
    /*07*/{(const char*)"fw_version", fw_version_get},
    /*08*/{(const char*)"restart", restart_device_get},
    /*09*/{(const char*)"heap_temperature", heap_temperature_get},
    /*10*/{(const char*)"activated", activated_get},
    /*11*/{(const char*)"format_sd_card", format_sd_card_get},
    /*12*/{(const char*)"ddns_client", ddns_client_get},
    /*13*/{(const char*)"pass_common", pass_common_get},
    /*14*/{(const char*)"format_spiffs", format_spiffs_get},
    /*15*/{(const char*)"reset_all_password", reset_all_password_get},
    /*16*/{(const char*)"reset_system_params", reset_system_params_get},
    /*17*/{(const char*)"clean_log_trace", clean_log_trace_get}
};

static server_post_handle_t client_post_handle[DATA_POST_HANDLE_NUM] = {
    /*00*/{(const char*)"sta_ap_info", sta_ap_info_post},
    /*01*/{(const char*)"sta_network", sta_network_post},
    /*02*/{(const char*)"sta_setting", sta_setting_post},
    /*03*/{(const char*)"ap_setting", ap_setting_post},
    /*04*/{(const char*)"sntp_setting", sntp_setting_post},
    /*05*/{(const char*)"device_info", device_info_post},
    /*06*/{(const char*)"auth_access", auth_access_post},
    /*07*/{(const char*)"time_setting", time_setting_post},
    /*08*/{(const char*)"ddns_client", ddns_client_post},
    /*09*/{(const char*)"auth_user_access", auth_user_access_post},
    /*10*/{(const char*)"pass_common", pass_common_post}
};

static void print_handleRequest(AsyncWebServerRequest *request, String &message);

WebServerURLHandle::WebServerURLHandle(const char *argName)
    : _argName(argName) {}
WebServerURLHandle::~WebServerURLHandle(){}

/**
 * Handler called after once request with method GET.
 */
int WebServerURLHandle::onHttpGet(AsyncWebServerRequest* request) 
{
    return onHttpGetAuth(request);
}

/**
 * Handler called after once request with method GET and authenticated.
 */
int WebServerURLHandle::onHttpGetAuth(AsyncWebServerRequest* request)
{
    bool isHandler = false;
    /* param wifi get 
     * /get?argName=arg(0)
     * using argName(0) = "param_wifi" is parameter to get wifi information 
     * using arg(0) is parameter to find callback_post
     */
    AsyncWebParameter* p = request->getParam(0);
    if (p->name() == _argName)
    {              
        for (uint8_t i = 0; i < DATA_GET_HANDLE_NUM; ++i)
        {
            if (p->value() == client_get_handle[i].path_arg)
            {
                HTTP_SERVER_URL_TAG_CONSOLE("get [%u]: argName = \"%s\"", i, client_get_handle[i].path_arg);
                if (client_get_handle[i].cb != nullptr) {
                    client_get_handle[i].cb(request, this);
                }
                isHandler = true;
                return i;
            }
        }
    }

    return -1;
#if (0)
    /* non process callback, so return arg parse */
    if (!isHandler)
    {
        String arg_str = "";
        print_handleRequest(request, arg_str);
        request->send(200, "text/html", arg_str);
    }
#endif
}

/**
 * Handler called after once request with method POST and authenticated.
 */
int WebServerURLHandle::onHttpPostAuth(AsyncWebServerRequest* request)
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
            HTTP_SERVER_URL_TAG_CONSOLE("post [%u]: argName = \"%s\"", i, client_post_handle[i].path_arg);
            if (client_post_handle[i].cb != nullptr) {
                client_post_handle[i].cb(request, this);
            }
            isHandler = true;
            return i;
        }
    }
    return -1;
#if (0)
    /* non process callback, so return arg parse */
    if (!isHandler)
    {
        String arg_str = "";
        print_handleRequest(request, arg_str);
        request->send(200, "text/html", arg_str);
    }
#endif
}

/*---------------------------------------------------------------------*
 *----------------------------data get process-------------------------*
 *---------------------------------------------------------------------*/
void sta_ap_info_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    IPAddress local_ip(0,0,0,0);
    uint8_t connect_st = 0;

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
        if(ETH.linkUp())
        {
            SERVER_DATA_CONSOLE("\r\nETH.linkUp OK");
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
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    root.printTo(*response);
    request->send(response);
}

/* Get json sta_network */
void sta_network_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
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

void sta_setting_get(AsyncWebServerRequest *request, WebServerURLHandle* client) 
{
    IPAddress local_ip(0,0,0,0);
    IPAddress gateway_ip(0,0,0,0);
    IPAddress subnet_ip(0,0,0,0);
    IPAddress dns_ip(0,0,0,0);
    uint8_t connect_st = 0;

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(Ethernet.isEnable())
    {
        if(ETH.linkUp())
        {
            SERVER_DATA_CONSOLE("\r\nETH.linkUp OK");
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

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    root.printTo(*response);
    request->send(response);
}

void ap_setting_get(AsyncWebServerRequest *request, WebServerURLHandle* client) 
{
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["ap_ssid"].set(ESPConfig.ssidAP());
    root["ap_pass"].set(ESPConfig.passAP());
    root["ap_dns_name"].set(ESPConfig.dnsNameAP()); 
    root["ap_on"].set((ESPConfig.isDisableAP() == 0) ? 1 : 0);  
    root["ap_channel"].set(ESPConfig.channelAP());
    root["ap_hidden"].set(ESPConfig.isHiddenAP());

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    root.printTo(*response);
    request->send(response);
}

void sntp_setting_get(AsyncWebServerRequest *request, WebServerURLHandle* client) 
{
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["server1"].set(ESPConfig.server1SNTP());
    root["server2"].set(ESPConfig.server2SNTP());
    root["server3"].set(ESPConfig.server3SNTP());
    root["gmtOffset"].set(ESPConfig.gmtOffsetSNTP());
    root["daylightOffset"].set(ESPConfig.daylightOffsetSNTP());

    AsyncResponseStream *response = request->beginResponseStream("text/json");
    root.printTo(*response);
    request->send(response);
}

void device_info_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["name"].set(ESPConfig.nameDevice());
    root["addr"].set(ESPConfig.addrDevice());

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    root.printTo(*response);
    request->send(response);
}

void time_setting_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{    
    rtc_time_t rtc;

    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();  

    rtc = ESPTime.get();
    if (rtc.year >= 2023)
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

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    root.printTo(*response);
    request->send(response);
}

/* /get?param_wifi=fw_version */
void fw_version_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["Hw_Version"].set(HW_VERSION_STRING);
    root["Fw_Version"].set(FW_VERSION_STRING);
    root["Stm8_Version"].set(STM8_FW_VERSION_STRING);
    root["Build_Date"].set(build_time);
    AsyncResponseStream *response = request->beginResponseStream("text/json");
    root.prettyPrintTo(*response);
    request->send(response);
}

/** /get?param_wifi=restart&device=X&value=Y
 * [X] = esp
 * [X] = stm8
 * [X] = main
 * 
 * Case of Stm8 and main
 * [Y] = 0 - (I2C_DEVICE_NUM - 1)
 * [Y] = 255(DEVICE_FORCE_RST_ALL) : restart all device
 * 
 * Case of esp32
 * [Y] : timeout for restart
 * 
 * Ex: 192.168.1.101:25130/get?param_wifi=restart&device=esp&value=255
*/
void restart_device_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    if (request->hasArg("device") && request->hasArg("value"))
    {
        String resp = "";
        String device = request->arg("device").c_str();
        int value = atoi(request->arg("value").c_str());
        boolean status;

        if (device == "esp") {
            value = max(500, value); /* Limited minimum value is 500ms */
            SOFTReset.enable(value, ESPSoftReset::HOME_PAGE_TYPE);
            resp = "Restart esp32";
        }
        request->send(200, "text/html", resp);
    }
    else {
        /* Handle for old version */
        request->send(200, "text/json", "Restart esp32");
        SOFTReset.enable(500, ESPSoftReset::HOME_PAGE_TYPE); /* Add a large timeout to make sure stm8 is being finished reset */
    }
}

/* /get?param_wifi=heap_temperature */
void heap_temperature_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    SERVER_DATA_CONSOLE("\r\nHeap in: %u", ESP.getFreeHeap());
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(esp32_internal_temp());
    json += ", \"gpio\":1";
    json += "}";
    request->send(200, "text/json", json);
    json = String();
    SERVER_DATA_CONSOLE("\r\nHeap out: %u", ESP.getFreeHeap());
}

/* /get?param_wifi=activated&cmd=X 
 * [X] = 0: InActive
 * [X] = 1: Active
 * [X] > 1: Read status
 */
void activated_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
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
        request->send(200, "text/html", "Value: " + String(EEPParams.isDeviceActivated()));
    }    
}

void format_sd_card_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    if (request->argName(1) == "pass" && ESPConfig.passConfirmIsOK(request->arg(1), ESPSysParams::CONFIRM_COMMON))
    {
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)    
        FSHandle.format(SD_FS_SYSTEM, "/");
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

void ddns_client_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();   
    root["service"].set(ESPConfig.serviceDDNS());
    root["domain"].set(ESPConfig.domainDDNS());
    root["user"].set(ESPConfig.userDDNS());  
    root["pass"].set(ESPConfig.passDDNS());
    root["ipUrl"].set(ESPConfig.ipUrlDDNS());
    root["sync_time"].set(ESPConfig.syncTimeDDNS());
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)    
    root["ip_ddns"].set(EasyDDNS.ddnsIP());
#else
    root["ip_ddns"].set("Disable");
#endif
    root["disable"].set(ESPConfig.disableDDNS());

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    root.printTo(*response);
    request->send(response);
}

void pass_common_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
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

void format_spiffs_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
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

void reset_all_password_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    request->send(200, "text/json", "Reset all password succeed!");
    ESPConfig.resetPassword();
}

void reset_system_params_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    request->send(200, "text/json", "Reset system params succeed!");
    ESPConfig.setDefault();
}

void clean_log_trace_get(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    request->send(200, "text/json", "Clean log trace succeed!");
    FS_LOG_CLEAN();
}

/*---------------------------------------------------------------------*
 *----------------------------data post process------------------------*
 *---------------------------------------------------------------------*/
void sta_ap_info_post(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
}

/* Post json sta_network 
{
"sta_ssid": "Quyen_2.4G",
"sta_ssid": "12345679",
"access_code": "1234"
}
*/
void sta_network_post(AsyncWebServerRequest *request, WebServerURLHandle* client) 
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Setting Succeed");

        ESPConfig.ssidSTASet(root["sta_ssid"].as<String>());
        ESPConfig.passSTASet(root["sta_pass"].as<String>());
        SERVER_DATA_CONSOLE("\r\nSSID: %s", ESPConfig.ssidSTA().c_str());
        SERVER_DATA_CONSOLE("\r\nPASS: %s\r\n", ESPConfig.passSTA().c_str());        
        ESPConfig.save();

        /* Reset to access new network */
        SOFTReset.enable(500, ESPSoftReset::STA_INFO_SETTING_TYPE);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void sta_setting_post(AsyncWebServerRequest *request, WebServerURLHandle* client)
{
    AsyncWebParameter* p = request->getParam(0);  

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_CONSOLE("JSON parsing failed!");
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
        ESPConfig.snSTASet(root["sta_sm"].as<String>());
        ESPConfig.dnsSTASet(root["sta_dns"].as<String>());
        ESPConfig.dhcpSTASet(root["sta_dhcp"].as<uint8_t>());
        ESPConfig.udpPortSet(root["udp_port"].as<uint16_t>());
        ESPConfig.tcpPortSet(root["tcp_port"].as<uint16_t>());
        ESPConfig.wsPortSet(root["ws_port"].as<uint16_t>()); 
        ESPConfig.disableSTASet(!root["sta_on"].as<uint8_t>());

        ESPConfig.save();

        /* Reset to access new network */
        SOFTReset.enable(500, ESPSoftReset::STA_INFO_SETTING_TYPE);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void ap_setting_post(AsyncWebServerRequest *request, WebServerURLHandle* client) 
{ 
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");

        ESPConfig.ssidAPSet(root["ap_ssid"].as<String>());
        ESPConfig.passAPSet(root["ap_pass"].as<String>());
        ESPConfig.dnsNameAPSet(root["ap_dns_name"].as<String>());
        ESPConfig.disableAPSet(!root["ap_on"].as<uint8_t>());
        ESPConfig.channelAPSet(root["ap_channel"].as<uint8_t>());
        ESPConfig.hiddenAPSet(root["ap_hidden"].as<uint8_t>()); 
        ESPConfig.save();

        /* Reset to access new network */
        SOFTReset.enable(500, ESPSoftReset::AP_INFO_SETTING_TYPE);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void sntp_setting_post(AsyncWebServerRequest *request, WebServerURLHandle* client) 
{
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        HTTP_SERVER_URL_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "SNTP Setting Succeed");

        ESPConfig.server1SNTPSet(root["server1"].as<String>());
        ESPConfig.server2SNTPSet(root["server2"].as<String>());
        ESPConfig.server3SNTPSet(root["server3"].as<String>());
        ESPConfig.gmtOffsetSNTPSet(root["gmtOffset"].as<int>());
        ESPConfig.daylightOffsetSNTPSet(root["daylightOffset"].as<int>());
        
        ESPConfig.save();
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void device_info_post(AsyncWebServerRequest *request, WebServerURLHandle* client)
{  
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Wifi Advance Setting Succeed");

        ESPConfig.nameDeviceSet(root["name"].as<String>());      
        ESPConfig.addrDeviceSet(root["addr"].as<String>());        
        ESPConfig.save();

        /* Reset to access new network */
        SOFTReset.enable(500, ESPSoftReset::DEVICE_INFO_NAME_TYPE);
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void auth_access_post(AsyncWebServerRequest *request, WebServerURLHandle* client)
{  
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        if (root["old_pass"] == ESPConfig.authAdminPass())
        {
            request->send(200, "text/json", "Đổi mật khẩu thành công");
            ESPConfig.authAdminPassSet(root["new_pass"].as<String>());        
            ESPConfig.save();
            /* Reset to access new network */
            SOFTReset.enable(500, ESPSoftReset::DEVICE_AUTH_SETTING_TYPE);
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

void time_setting_post(AsyncWebServerRequest *request, WebServerURLHandle* client)
{    
    AsyncWebParameter* p = request->getParam(0);
    const char *rtc_str = p->value().c_str();
    if (ESPTime.GMTStringUpdate(rtc_str, ESPTimeSystem::ESP_RTC_WEB_UPDATE))
    {
        request->send(200, "text/json", "Time Setting Succeed");
    }
    else
    {
        request->send(200, "text/json", "Time Setting Wrong");
    }    
}

void ddns_client_post(AsyncWebServerRequest *request, WebServerURLHandle* client) 
{   
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "DDNS Setting Succeed");

        ESPConfig.serviceDDNSSet(root["service"].as<String>());
        ESPConfig.domainDDNSSet(root["domain"].as<String>());
        ESPConfig.userDDNSSet(root["user"].as<String>());
        ESPConfig.passDDNSSet(root["pass"].as<String>());
        ESPConfig.ipUrlDDNSSet(root["ipUrl"].as<String>());
        ESPConfig.syncTimeDDNSSet(root["sync_time"].as<int>());  
        ESPConfig.disableDDNSSet(root["disable"].as<int>()); 
        ESPConfig.save();
#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)        
        EasyDDNS.service(ESPConfig.serviceDDNS());
        EasyDDNS.urlGetIp(ESPConfig.ipUrlDDNS());
        EasyDDNS.begin(ESPConfig.domainDDNS(), ESPConfig.userDDNS(), ESPConfig.passDDNS());
#endif        
    }
    else
    {
        request->send(200, "text/json", "Password Setting Wrong");
    }
}

void auth_user_access_post(AsyncWebServerRequest *request, WebServerURLHandle* client)
{  
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["access_code"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        if (root["old_pass"] == ESPConfig.authUserPass())
        {
            request->send(200, "text/json", "Đổi mật khẩu thành công");
            ESPConfig.authUserPassSet(root["new_pass"].as<String>());      
            ESPConfig.save();

            /* Reset to access new network */
            SOFTReset.enable(500, ESPSoftReset::DEVICE_AUTH_SETTING_TYPE);
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

void pass_common_post(AsyncWebServerRequest *request, WebServerURLHandle* client)
{ 
    AsyncWebParameter* p = request->getParam(0);

    DynamicJsonBuffer djbpo;
    JsonObject& root = djbpo.parseObject(p->value());
    if (!root.success())
    {
        SERVER_DATA_CONSOLE("JSON parsing failed!");
        return;
    }

    if(ESPConfig.passConfirmIsOK(root["old_pass"].as<String>(), ESPSysParams::CONFIRM_COMMON))
    {
        request->send(200, "text/json", "Đổi mật khẩu thành công");
        ESPConfig.passConfirmSet(ESPSysParams::CONFIRM_COMMON, root["new_pass"].as<String>().toInt());
        ESPConfig.save();
    }
    else
    {
        request->send(200, "text/json", "Old pass Wrong");
    }
}

void print_handleRequest(AsyncWebServerRequest *request, String &message)
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
  SERVER_DATA_PORT.println(message);
}