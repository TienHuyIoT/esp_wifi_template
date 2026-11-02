#include <ArduinoJson.h>
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_PFTimeZone.h"
#include "THIoT_PFcUtil.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFSerialTrace.h"

/* Firmware revision stored in the parameter file.  "V<major>.<minor>.<build>"
 * is packed as <major>*100 + <minor>*10 + <build> so the loaded file can be
 * compared with the running firmware and refreshed when it is older. */
#ifndef FW_VERSION_INT
#define FW_VERSION_INT          ((FW_VERSION_MAJOR * 100) + (FW_VERSION_MINOR * 10) + FW_VERSION_BUILD)
#endif

#define SYS_PARAM_PORT              SERIAL_PORT
#define SYS_PARAM_TAG_CONSOLE(...)  //CONSOLE_TAG_LOGI("[SYS_PARAM]", __VA_ARGS__)

#define SYS_PARAM_TAG_ESP_LOG(...)  ESP_FS_TAG_LOGI("[SYS_PARAM]", __VA_ARGS__)

/* JsonBuffer capacity of the parameter file (ArduinoJson v5 object sizes):
 *   root(13) = port(3) device(3) auth(2) auth_user(2) sta(10) ap(8) ddns(8)
 *              sntp(7) fw_version version + confirm[5] + ping[2]{5} */
#define PARAM_JSON_OBJ_SIZE (JSON_ARRAY_SIZE(2) + JSON_ARRAY_SIZE(5) + 2*JSON_OBJECT_SIZE(2) + 2*JSON_OBJECT_SIZE(3) + 2*JSON_OBJECT_SIZE(5) + 3*JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(10) + JSON_OBJECT_SIZE(13))

const char espSysParamsDefault[] PROGMEM = R"=====(
{
    "port":{
        "udp": 25123,
        "tcp": 25123,
        "ws" : 25124
    },
    "device":{
        "name": "ESP32 WIFI TEMPLATE",
        "addr": "N/A",
        "tell": "N/A"
    },
    "auth":{
        "user": "admin",
        "pass": "admin"
    },
    "auth_user":{
        "user": "admin",
        "pass": "12345"
    },
    "confirm":[
        1234,
        1234,
        1234,
        1234,
        1234
    ],
    "sta":{        
        "ip":"192.168.1.100",
        "gw":"192.168.1.1",
        "sn":"255.255.255.0",
        "dns":"192.168.1.1",
        "ssid":"",
        "psk":"",
        "hostname":"esp32template",
        "dhcp":1,
        "disable":0,
        "smart_cfg":1
    },
    "ap":{        
        "ip":"192.168.4.1",
        "sn":"255.255.255.0",
        "ssid":"ESP32_TEMPLATE",
        "psk":"88888888",
        "dns_name":"esp32template.vn",        
        "disable":0,
        "channel":2,
        "hidden":0
    },
    "ddns":{
        "service": "noip",
        "server": "dynupdate.no-ip.com",
        "ipUrl": "ifconfig.me/ip",
        "domain": "example.com",
        "user": "admin",
        "pass": "admin",
        "sync_time": 120,
        "disable": 1
    },
    "sntp":{
        "server1": "pool.ntp.org",
        "server2": "time.nist.gov",
        "server3": "time.windows.com",
        "gmtOffset": 7,
        "daylightOffset": 0,
        "TzTime": "<+07>-7",
        "interval": 3600
    },
    "ping":[
        {"host": "gateway", "timeout": 180, "interval": 60, "type": 1, "enable": 1},
        {"host": "google.com", "timeout": 180, "interval": 60, "type": 1, "enable": 0}
    ],
    "fw_version": 318,
    "version":2
}
)=====" ;


ESPSysParams::ESPSysParams(FS &fs)
:_fs(&fs)
{
}

ESPSysParams::~ESPSysParams()
{
}

void ESPSysParams::load(FS* fs)
{
    if (fs != nullptr)
    {
        _fs = fs;
    }

    uint8_t status = syncFromFileSystem();
    if (UPDATE_STATUS == status) {
        saveToFileSystem();
    } else if (ERROR_STATUS == status) {
        SYS_PARAM_TAG_ESP_LOG("[ERROR] syncFromFileSystem");
        delay(100);
        /* Due to Reset factory the file configure default */
        ESP.restart();
    }
}

void ESPSysParams::resetPassword()
{
    strncpy(_sys_prams.auth_admin.user, (const char *)"admin", AUTH_LENGTH_MAX);
    strncpy(_sys_prams.auth_admin.pass, (const char *)"admin", AUTH_LENGTH_MAX);
    strncpy(_sys_prams.auth_user.user, (const char *)"admin", AUTH_LENGTH_MAX);
    strncpy(_sys_prams.auth_user.pass, (const char *)"12345", AUTH_LENGTH_MAX);
    _sys_prams.confirm[CONFIRM_COMMON] = PASS_COMMON_DEFAULT;
    _sys_prams.confirm[CONFIRM_PU3] = PASS_PU3_DEFAULT;
    _sys_prams.confirm[CONFIRM_COST] = PASS_COST_DEFAULT;
    _sys_prams.confirm[CONFIRM_LOGFILE] = PASS_LOGFILE_DEFAULT;
    saveToFileSystem();
}

boolean ESPSysParams::passSupperAdminIsOK(const String &pass)
{
    return (pass == "25251325");
}

boolean ESPSysParams::passConfirmIsOK(const String &pass, passConfirm_t type)
{
    boolean result = false;
    if(pass == String(_sys_prams.confirm[type]) || passSupperAdminIsOK(pass))
    {
        result = true;
    }
    return result;
}

void ESPSysParams::saveToFileSystem()
{
    using File = fs::File;
    File fs_handle;

    const size_t capacity = PARAM_JSON_OBJ_SIZE;
    DynamicJsonBuffer jsonBuffer(capacity);
    JsonObject& root = jsonBuffer.createObject();

    JsonObject& port = root.createNestedObject("port");
    port["udp"].set(_sys_prams.port.udp);
    port["tcp"].set(_sys_prams.port.tcp);
    port["ws"].set(_sys_prams.port.ws);

    JsonObject& device = root.createNestedObject("device");
    device["name"].set(_sys_prams.device.name);
    device["addr"].set(_sys_prams.device.addr);
    device["tell"].set(_sys_prams.device.tell);

    JsonObject& auth = root.createNestedObject("auth");
    auth["user"].set(_sys_prams.auth_admin.user);
    auth["pass"].set(_sys_prams.auth_admin.pass);

    JsonObject& auth_user = root.createNestedObject("auth_user");
    auth_user["user"].set(_sys_prams.auth_user.user);
    auth_user["pass"].set(_sys_prams.auth_user.pass);

    JsonArray& confirm = root.createNestedArray("confirm");
    for(uint8_t i = 0; i < CONFIRM_NUM_MAX; ++i)
    {
        confirm.add(_sys_prams.confirm[i]);
    }

    JsonObject& sta = root.createNestedObject("sta");
    sta["ip"].set(_sys_prams.sta.ip.toString());
    sta["gw"].set(_sys_prams.sta.gw.toString());
    sta["sn"].set(_sys_prams.sta.sn.toString());
    sta["dns"].set(_sys_prams.sta.dns.toString());
    sta["ssid"].set(_sys_prams.sta.ssid);
    sta["psk"].set(_sys_prams.sta.pass);
    sta["hostname"].set(_sys_prams.sta.hostname);
    sta["dhcp"].set(_sys_prams.sta.dhcp);
    sta["disable"].set(_sys_prams.sta.disable);
    sta["smart_cfg"].set(_sys_prams.sta.smart_cfg);

    JsonObject& ap = root.createNestedObject("ap");
    ap["ip"].set(_sys_prams.ap.ip.toString());
    ap["sn"].set(_sys_prams.ap.sn.toString());
    ap["ssid"].set(_sys_prams.ap.ssid);
    ap["psk"].set(_sys_prams.ap.pass);
    ap["dns_name"].set(_sys_prams.ap.dns_name);    
    ap["disable"].set(_sys_prams.ap.disable);
    ap["hidden"].set(_sys_prams.ap.hidden);
    ap["channel"].set(_sys_prams.ap.channel); 

    JsonObject& ddns = root.createNestedObject("ddns");
    ddns["service"].set(_sys_prams.ddns.service);
    ddns["server"].set(_sys_prams.ddns.server);
    ddns["domain"].set(_sys_prams.ddns.domain);
    ddns["user"].set(_sys_prams.ddns.user);    
    ddns["pass"].set(_sys_prams.ddns.pass); 
    ddns["sync_time"].set(_sys_prams.ddns.sync_time); 
    ddns["disable"].set(_sys_prams.ddns.disable);
    ddns["ipUrl"].set(_sys_prams.ddns.ipUrl);

    JsonObject& sntp = root.createNestedObject("sntp");
    sntp["server1"].set(_sys_prams.sntp.server1);
    sntp["server2"].set(_sys_prams.sntp.server2);
    sntp["server3"].set(_sys_prams.sntp.server3);
    sntp["TzTime"].set(_sys_prams.sntp.TzTime);
    sntp["gmtOffset"].set(_sys_prams.sntp.gmtOffset); 
    sntp["daylightOffset"].set(_sys_prams.sntp.daylightOffset);
    sntp["interval"].set(_sys_prams.sntp.interval);

    JsonArray& ping = root.createNestedArray("ping");
    JsonObject& ping_0 = ping.createNestedObject();
    JsonObject& ping_1 = ping.createNestedObject();
    ping_0["host"].set(_sys_prams.ping.host[PING_HOST1_INDEX]);
    ping_1["host"].set(_sys_prams.ping.host[PING_HOST2_INDEX]);
    ping_0["timeout"].set(_sys_prams.ping.timeout[PING_HOST1_INDEX]);
    ping_1["timeout"].set(_sys_prams.ping.timeout[PING_HOST2_INDEX]);
    ping_0["interval"].set(_sys_prams.ping.interval[PING_HOST1_INDEX]);
    ping_1["interval"].set(_sys_prams.ping.interval[PING_HOST2_INDEX]);
    ping_0["type"].set(_sys_prams.ping.type[PING_HOST1_INDEX]);
    ping_1["type"].set(_sys_prams.ping.type[PING_HOST2_INDEX]);
    ping_0["enable"].set(_sys_prams.ping.enable[PING_HOST1_INDEX]);
    ping_1["enable"].set(_sys_prams.ping.enable[PING_HOST2_INDEX]);

    root["fw_version"].set(_sys_prams.fw_version);
    root["version"].set(_sys_prams.version);

    SYS_PARAM_TAG_CONSOLE("Json created");
    // root.prettyPrintTo(SYS_PARAM_PORT);
    fs_handle = _fs->open(ESP_SYSTEM_PARAMS, FILE_WRITE);
    if (!fs_handle) {
        SYS_PARAM_TAG_CONSOLE("Failed to open file %s for writing", ESP_SYSTEM_PARAMS);
        return;
    }
    root.prettyPrintTo<File>(fs_handle);
    fs_handle.close();    
    SYS_PARAM_TAG_CONSOLE("wifi json info updated"); 
}

uint8_t ESPSysParams::syncFromFileSystem()
{
    File fs_handle;
    uint8_t status = OK_STATUS;

    if (!_fs->exists(ESP_SYSTEM_PARAMS))
    {
        fs_handle = _fs->open(ESP_SYSTEM_PARAMS, FILE_WRITE);
        if (!fs_handle) {
            SYS_PARAM_TAG_CONSOLE("Failed to open file %s for writing", ESP_SYSTEM_PARAMS);
            return ERROR_STATUS;
        }
        fs_handle.printf_P(espSysParamsDefault);
        fs_handle.close();
    }
    
    const size_t capacity = PARAM_JSON_OBJ_SIZE + 1500;
    DynamicJsonBuffer jsonBuffer(capacity);
    fs_handle = _fs->open(ESP_SYSTEM_PARAMS, FILE_READ);  
    if (!fs_handle) {
        SYS_PARAM_TAG_CONSOLE("Failed to open file %s for reading", ESP_SYSTEM_PARAMS);
        return ERROR_STATUS;
    }  
    JsonObject &root = jsonBuffer.parseObject(fs_handle);
    fs_handle.close();
    
    if (!root.success())
    {
        SYS_PARAM_TAG_CONSOLE("JSON parsing failed!");
        bitSet(_statusUpdate, ERROR_STATUS);

        fs_handle = _fs->open(ESP_SYSTEM_PARAMS, FILE_WRITE);
        if (!fs_handle) {
            SYS_PARAM_TAG_CONSOLE("Failed to open file %s for writing", ESP_SYSTEM_PARAMS);
            return ERROR_STATUS;
        }
        fs_handle.printf_P(espSysParamsDefault);
        fs_handle.close();

        return ERROR_STATUS;
    }

    JsonObject& port = root["port"];
    if (port.success())
    {
        _sys_prams.port.udp = port["udp"].as<int>();
        _sys_prams.port.tcp = port["tcp"].as<int>();
        _sys_prams.port.ws  = port["ws"].as<int>();
    }

    JsonObject& device = root["device"];
    if (device.success())
    {
        device["name"].as<String>().toCharArray(_sys_prams.device.name, DEVICE_NAME_LENGTH_MAX + 1);
        device["addr"].as<String>().toCharArray(_sys_prams.device.addr, DEVICE_ADDR_LENGTH_MAX + 1);
        device["tell"].as<String>().toCharArray(_sys_prams.device.tell, DEVICE_TELL_LENGTH_MAX + 1);
    }

    JsonObject& auth = root["auth"];
    if (auth.success())
    {
        auth["user"].as<String>().toCharArray(_sys_prams.auth_admin.user, AUTH_LENGTH_MAX + 1);
        auth["pass"].as<String>().toCharArray(_sys_prams.auth_admin.pass, AUTH_LENGTH_MAX + 1);
    }

    JsonObject& auth_user = root["auth_user"];
    if (auth_user.success())
    {
        auth_user["user"].as<String>().toCharArray(_sys_prams.auth_user.user, AUTH_LENGTH_MAX + 1);
        auth_user["pass"].as<String>().toCharArray(_sys_prams.auth_user.pass, AUTH_LENGTH_MAX + 1);
    }

    JsonArray& confirm = root["confirm"];
    if (confirm.success())
    {
        uint8_t num = min((int)(confirm.size()), CONFIRM_NUM_MAX);
        for(uint8_t i = 0; i < num; ++i)
        {
            _sys_prams.confirm[i] = confirm[i];
        }        
    }

    JsonObject& sta = root["sta"];
    if (sta.success())
    {
        SYS_PARAM_TAG_CONSOLE("Load sta information");
        _sys_prams.sta.ip.fromString(sta["ip"].as<String>());
        _sys_prams.sta.gw.fromString(sta["gw"].as<String>());
        _sys_prams.sta.sn.fromString(sta["sn"].as<String>());
        _sys_prams.sta.dns.fromString(sta["dns"].as<String>());
        sta["ssid"].as<String>().toCharArray(_sys_prams.sta.ssid, SSID_LENGTH_MAX + 1);
        sta["psk"].as<String>().toCharArray(_sys_prams.sta.pass, PASS_LENGTH_MAX + 1);
        sta["hostname"].as<String>().toCharArray(_sys_prams.sta.hostname, HOSTNAME_LENGTH_MAX + 1); 
        _sys_prams.sta.dhcp      = sta["dhcp"].as<int>();  
        _sys_prams.sta.disable   = sta["disable"].as<int>();
        _sys_prams.sta.smart_cfg = sta["smart_cfg"].as<int>();
    }

    JsonObject& ap = root["ap"];
    if (ap.success())
    {
        SYS_PARAM_TAG_CONSOLE("Load ap information");
        _sys_prams.ap.ip.fromString(ap["ip"].as<String>());
        _sys_prams.ap.sn.fromString(ap["sn"].as<String>()); 
        ap["ssid"].as<String>().toCharArray(_sys_prams.ap.ssid, SSID_LENGTH_MAX + 1);
        ap["psk"].as<String>().toCharArray(_sys_prams.ap.pass, PASS_LENGTH_MAX + 1);       
        ap["dns_name"].as<String>().toCharArray(_sys_prams.ap.dns_name, HOSTNAME_LENGTH_MAX + 1);
        _sys_prams.ap.disable = ap["disable"].as<int>();  
        _sys_prams.ap.channel = ap["channel"].as<int>();
        _sys_prams.ap.hidden  = ap["hidden"].as<int>(); 
    }  

    JsonObject& ddns = root["ddns"];
    if (ddns.success())
    {
        SYS_PARAM_TAG_CONSOLE("Load ddns information");
        ddns["service"].as<String>().toCharArray(_sys_prams.ddns.service, DDNS_SERVICE_LENGTH_MAX + 1);
        ddns["domain"].as<String>().toCharArray(_sys_prams.ddns.domain, DDNS_DOMAIN_LENGTH_MAX + 1);
        ddns["user"].as<String>().toCharArray(_sys_prams.ddns.user, DDNS_USER_LENGTH_MAX + 1);
        ddns["pass"].as<String>().toCharArray(_sys_prams.ddns.pass, DDNS_PASS_LENGTH_MAX + 1);
        _sys_prams.ddns.sync_time = ddns["sync_time"].as<int>();
        _sys_prams.ddns.disable = ddns["disable"].as<int>();
        if (ddns["ipUrl"].success()){
            ddns["ipUrl"].as<String>().toCharArray(_sys_prams.ddns.ipUrl, DDNS_IPURL_LENGTH_MAX + 1);
        } else{
            strncpy(_sys_prams.ddns.ipUrl, "ifconfig.me/ip", DDNS_IPURL_LENGTH_MAX + 1);
        }
        if (ddns["server"].success()){
            ddns["server"].as<String>().toCharArray(_sys_prams.ddns.server, DDNS_SERVER_LENGTH_MAX + 1);
        } else{
            strncpy(_sys_prams.ddns.server, "dynupdate.no-ip.com", DDNS_SERVER_LENGTH_MAX + 1);
        }
    }

    JsonObject& sntp = root["sntp"];
    if (sntp.success())
    {
        SYS_PARAM_TAG_CONSOLE("Load sntp information");
        sntp["server1"].as<String>().toCharArray(_sys_prams.sntp.server1, SNTP_SERVER_LENGTH_MAX + 1);
        sntp["server2"].as<String>().toCharArray(_sys_prams.sntp.server2, SNTP_SERVER_LENGTH_MAX + 1);
        sntp["server3"].as<String>().toCharArray(_sys_prams.sntp.server3, SNTP_SERVER_LENGTH_MAX + 1);
        _sys_prams.sntp.gmtOffset = sntp["gmtOffset"].as<int>();
        _sys_prams.sntp.daylightOffset = sntp["daylightOffset"].as<int>();
        _sys_prams.sntp.interval = sntp["interval"].as<int>();
        if (sntp["TzTime"].success()) {
            sntp["TzTime"].as<String>().toCharArray(_sys_prams.sntp.TzTime, SNTP_TZ_LENGTH_MAX + 1);
        }
        else {
            strncpy(_sys_prams.sntp.TzTime, TZ_Asia_Ho_Chi_Minh, SNTP_TZ_LENGTH_MAX + 1);
        }
    }
    else
    {
        strncpy(_sys_prams.sntp.server1, "pool.ntp.org", SNTP_SERVER_LENGTH_MAX + 1);
        strncpy(_sys_prams.sntp.server2, "time.nist.gov", SNTP_SERVER_LENGTH_MAX + 1);
        strncpy(_sys_prams.sntp.server3, "time.windows.com", SNTP_SERVER_LENGTH_MAX + 1);
        strncpy(_sys_prams.sntp.TzTime, TZ_Asia_Ho_Chi_Minh, SNTP_TZ_LENGTH_MAX + 1);
        _sys_prams.sntp.gmtOffset = 7;
        _sys_prams.sntp.daylightOffset = 0;
        _sys_prams.sntp.interval = 3600;
    }

    JsonObject& ping_0 = root["ping"][PING_HOST1_INDEX];
    JsonObject& ping_1 = root["ping"][PING_HOST2_INDEX];
    if (ping_0.success() && ping_1.success())
    {
        SYS_PARAM_TAG_CONSOLE("Load ping information");
        ping_0["host"].as<String>().toCharArray(_sys_prams.ping.host[PING_HOST1_INDEX], PING_HOST_LENGTH_MAX + 1);
        ping_1["host"].as<String>().toCharArray(_sys_prams.ping.host[PING_HOST2_INDEX], PING_HOST_LENGTH_MAX + 1);
        _sys_prams.ping.timeout[PING_HOST1_INDEX] = ping_0["timeout"].as<int>();
        _sys_prams.ping.timeout[PING_HOST2_INDEX] = ping_1["timeout"].as<int>();
        _sys_prams.ping.interval[PING_HOST1_INDEX] = ping_0["interval"].as<int>();
        _sys_prams.ping.interval[PING_HOST2_INDEX] = ping_1["interval"].as<int>();
        _sys_prams.ping.type[PING_HOST1_INDEX] = ping_0["type"].as<int>();
        _sys_prams.ping.type[PING_HOST2_INDEX] = ping_1["type"].as<int>();
        _sys_prams.ping.enable[PING_HOST1_INDEX] = ping_0["enable"].as<bool>();
        _sys_prams.ping.enable[PING_HOST2_INDEX] = ping_1["enable"].as<bool>();
    }
    else
    {
        strncpy(_sys_prams.ping.host[PING_HOST1_INDEX], "gateway", PING_HOST_LENGTH_MAX + 1);
        strncpy(_sys_prams.ping.host[PING_HOST2_INDEX], "google.com", PING_HOST_LENGTH_MAX + 1);
        _sys_prams.ping.timeout[PING_HOST1_INDEX] = 180;
        _sys_prams.ping.timeout[PING_HOST2_INDEX] = 180;
        _sys_prams.ping.interval[PING_HOST1_INDEX] = 60;
        _sys_prams.ping.interval[PING_HOST2_INDEX] = 60;
        _sys_prams.ping.type[PING_HOST1_INDEX] = 1;
        _sys_prams.ping.type[PING_HOST2_INDEX] = 1;
        _sys_prams.ping.enable[PING_HOST1_INDEX] = 1;
        _sys_prams.ping.enable[PING_HOST2_INDEX] = 0;
    }

    /** Adapter for V3.1.6 */
    if (root["version"].success()) {
        _sys_prams.version = root["version"].as<uint8_t>();
        if (_sys_prams.version < SYS_PARAM_VERSION) {
            _sys_prams.version = SYS_PARAM_VERSION;
            bitSet(_statusUpdate, NEW_VERSION_STATUS);
            status = UPDATE_STATUS;
        }
    } else {
        _sys_prams.version = SYS_PARAM_VERSION;

        /** Update ping timeout */
        _sys_prams.ping.timeout[PING_HOST1_INDEX] = 180;
        _sys_prams.ping.timeout[PING_HOST2_INDEX] = 180;
        _sys_prams.ping.interval[PING_HOST1_INDEX] = 60;
        _sys_prams.ping.interval[PING_HOST2_INDEX] = 60;
        bitSet(_statusUpdate, NEW_VERSION_STATUS);
        bitSet(_statusUpdate, ADD_VERION_STATUS);
        status = UPDATE_STATUS;
    }

    /** Adapter for V3.1.6 */
    if (root["fw_version"].success()) {
        _sys_prams.fw_version = root["fw_version"].as<int>();
        if (_sys_prams.fw_version < FW_VERSION_INT) {
            _sys_prams.fw_version = FW_VERSION_INT;
            bitSet(_statusUpdate, NEW_FIRMWARE_STATUS);
            status = UPDATE_STATUS;
        }
    } else {
        _sys_prams.fw_version = FW_VERSION_INT;
        bitSet(_statusUpdate, NEW_FIRMWARE_STATUS);
        bitSet(_statusUpdate, ADD_FIRMWARE_STATUS);
        status = UPDATE_STATUS;
    }

    SYS_PARAM_TAG_CONSOLE("Load succeed status = %u", status);
    if (UPDATE_STATUS == status) {
        bitSet(_statusUpdate, UPDATE_STATUS);
    } else {
        bitSet(_statusUpdate, OK_STATUS);
    }
    return status;
}

ESPSysParams ESPConfig(NAND_FS_SYSTEM);
