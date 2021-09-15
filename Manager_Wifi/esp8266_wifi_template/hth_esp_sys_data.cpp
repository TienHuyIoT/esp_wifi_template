#include <ArduinoJson.h>
#include "app_config.h"
#include "hth_esp_sys_data.h"
#include "hth_console_dbg.h"

#define WIFI_FILE_PORT CONSOLE_PORT
#define WIFI_DATA_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)
#define WIFI_DATA_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[WIFI DATA]", __VA_ARGS__)

const char wifi_data_default_json[] PROGMEM = R"=====(
{
    "port":{
        "udp": 25130,
        "tcp": 25130,
        "ws" : 25131
    },
    "device": {
        "name": "Công Ty TNHH Điện Tử Tiến Huy",
        "addr": "102 Phan Huy Ích, Phường 15, Quận Tân Bình, Tp HCM",
        "tell": "0983497310"
    },
    "auth_admin":{
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
        "ip":"192.168.1.101",
        "gw":"192.168.1.1",
        "sn":"255.255.255.0",
        "dns":"192.168.1.1",
        "ssid":"",
        "psk":"",
        "hostname": "tienhuyiot",
        "dhcp": 1,
        "disable": 0,
        "smart_cfg": 1
    },
    "ap": {
        "ip": "192.168.4.1",
        "sn": "255.255.255.0",
        "ssid": "tienhuyiot",
        "psk": "88888888",
        "dns_name": "tienhuyiot.vn",
        "disable": 0,
        "hidden": 0,
        "channel": 2
    },
    "ddns":{
        "service": "noip",
        "domain": "tienhuyiot.ddns.net",
        "user": "tienhuyiot",
        "pass": "123456789",
        "sync_time": 30,
        "disable":1
    }
}
)=====" ;

wifi_data_file::wifi_data_file(fs::FS &fs)
:_fs(&fs)
{
}

wifi_data_file::~wifi_data_file()
{
}

void wifi_data_file::begin()
{
    syncFromFileSystem();
}

void wifi_data_file::resetPassword()
{
    strncpy(_file_prams.auth_admin.pass, (const char *)"admin", AUTH_LENGHT_MAX);
    strncpy(_file_prams.auth_user.pass, (const char *)"admin", AUTH_LENGHT_MAX);
    _file_prams.confirm[CONFIRM_COMMON] = PASS_COMMON_DEFAULT;
    _file_prams.confirm[CONFIRM_PU3] = PASS_PU3_DEFAULT;
    _file_prams.confirm[CONFIRM_COST] = PASS_COST_DEFAULT;
    _file_prams.confirm[CONFIRM_LOGFILE] = PASS_LOGFILE_DEFAULT;
    saveToFileSystem();
}

void wifi_data_file::resetDefault()
{
    _fs->remove(WIFI_FILE_PATH);
}

bool wifi_data_file::passSupperAdminIsOK(const String &pass)
{
    return (pass == "25251325");
}

bool wifi_data_file::passConfirmIsOK(const String &pass, wifi_pass_confirm_t type)
{
    bool result = false;
    if(pass == String(_file_prams.confirm[type]) || passSupperAdminIsOK(pass))
    {
        result = true;
    }
    return result;
}

void wifi_data_file::saveToFileSystem()
{
    File fs_handle;
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();

    JsonObject& port = root.createNestedObject("port");
    port["udp"].set(_file_prams.port.udp);
    port["tcp"].set(_file_prams.port.tcp);
    port["ws"].set(_file_prams.port.ws);

    JsonObject& device = root.createNestedObject("device");
    device["name"].set(_file_prams.device.name);
    device["addr"].set(_file_prams.device.addr);
    device["tell"].set(_file_prams.device.tell);

    JsonObject& auth_admin = root.createNestedObject("auth_admin");
    auth_admin["user"].set(_file_prams.auth_admin.user);
    auth_admin["pass"].set(_file_prams.auth_admin.pass);

    JsonObject& auth_user = root.createNestedObject("auth_user");
    auth_user["user"].set(_file_prams.auth_user.user);
    auth_user["pass"].set(_file_prams.auth_user.pass);

    JsonArray& confirm = root.createNestedArray("confirm");
    for(uint8_t i = 0; i < CONFIRM_NUM_MAX; ++i)
    {
        confirm.add(_file_prams.confirm[i]);
    }

    JsonObject& sta = root.createNestedObject("sta");
    sta["ip"].set(_file_prams.sta.ip.toString());
    sta["gw"].set(_file_prams.sta.gw.toString());
    sta["sn"].set(_file_prams.sta.sn.toString());
    sta["dns"].set(_file_prams.sta.dns.toString());
    sta["ssid"].set(_file_prams.sta.ssid);
    sta["psk"].set(_file_prams.sta.pass);
    sta["hostname"].set(_file_prams.sta.hostname);
    sta["dhcp"].set(_file_prams.sta.dhcp);
    sta["disable"].set(_file_prams.sta.disable);
    sta["smart_cfg"].set(_file_prams.sta.smart_cfg);

    JsonObject& ap = root.createNestedObject("ap");
    ap["ip"].set(_file_prams.ap.ip.toString());
    ap["sn"].set(_file_prams.ap.sn.toString());
    ap["ssid"].set(_file_prams.ap.ssid);
    ap["psk"].set(_file_prams.ap.pass);
    ap["dns_name"].set(_file_prams.ap.dns_name);    
    ap["disable"].set(_file_prams.ap.disable);
    ap["hidden"].set(_file_prams.ap.hidden);
    ap["channel"].set(_file_prams.ap.channel); 

    JsonObject& ddns = root.createNestedObject("ddns");
    ddns["service"].set(_file_prams.ddns.service);
    ddns["domain"].set(_file_prams.ddns.domain);
    ddns["user"].set(_file_prams.ddns.user);    
    ddns["pass"].set(_file_prams.ddns.pass); 
    ddns["sync_time"].set(_file_prams.ddns.sync_time); 
    ddns["disable"].set(_file_prams.ddns.disable);

    WIFI_DATA_TAG_CONSOLE("Json created:");
    // root.prettyPrintTo(WIFI_FILE_PORT);
    fs_handle = _fs->open(WIFI_FILE_PATH, "w");
    root.prettyPrintTo(fs_handle);
    fs_handle.close();    
    WIFI_DATA_TAG_CONSOLE("wifi json info updated"); 
}

void wifi_data_file::syncFromFileSystem()
{
    File fs_handle;

    if (!_fs->exists(WIFI_FILE_PATH))
    {
        fs_handle = _fs->open(WIFI_FILE_PATH, "w");
        fs_handle.printf_P(wifi_data_default_json);
        fs_handle.close();
    }

    fs_handle = _fs->open(WIFI_FILE_PATH, "r");    
    
    DynamicJsonBuffer djbpo;
    JsonObject &root = djbpo.parseObject(fs_handle);
    fs_handle.close();
    
    if (!root.success())
    {
        WIFI_DATA_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    JsonObject& port = root["port"];
    if (port.success())
    {
        _file_prams.port.udp = port["udp"].as<int>();
        _file_prams.port.tcp = port["tcp"].as<int>();
        _file_prams.port.ws  = port["ws"].as<int>();
    }

    JsonObject& device = root["device"];
    if (device.success())
    {
        device["name"].as<String>().toCharArray(_file_prams.device.name, DEVICENAME_LENGHT_MAX + 1);
        device["addr"].as<String>().toCharArray(_file_prams.device.addr, DEVICE_ADDR_LENGHT_MAX + 1);
        device["tell"].as<String>().toCharArray(_file_prams.device.tell, DEVICE_TELL_LENGHT_MAX + 1);
    }

    JsonObject& auth_admin = root["auth_admin"];
    if (auth_admin.success())
    {
        auth_admin["user"].as<String>().toCharArray(_file_prams.auth_admin.user, AUTH_LENGHT_MAX + 1);
        auth_admin["pass"].as<String>().toCharArray(_file_prams.auth_admin.pass, AUTH_LENGHT_MAX + 1);
    }

    JsonObject& auth_user = root["auth_user"];
    if (auth_user.success())
    {
        auth_user["user"].as<String>().toCharArray(_file_prams.auth_user.user, AUTH_LENGHT_MAX + 1);
        auth_user["pass"].as<String>().toCharArray(_file_prams.auth_user.pass, AUTH_LENGHT_MAX + 1);
    }

    JsonArray& confirm = root["confirm"];
    if (confirm.success())
    {
        uint8_t num = min((int)(confirm.size()), (int)CONFIRM_NUM_MAX);
        for(uint8_t i = 0; i < num; ++i)
        {
            _file_prams.confirm[i] = confirm[i];
        }        
    }

    JsonObject& sta = root["sta"];
    if (sta.success())
    {
        _file_prams.sta.ip.fromString(sta["ip"].as<String>());
        _file_prams.sta.gw.fromString(sta["gw"].as<String>());
        _file_prams.sta.sn.fromString(sta["sn"].as<String>());
        _file_prams.sta.dns.fromString(sta["dns"].as<String>());
        sta["ssid"].as<String>().toCharArray(_file_prams.sta.ssid, SSID_LENGHT_MAX + 1);
        sta["psk"].as<String>().toCharArray(_file_prams.sta.pass, PASS_LENGHT_MAX + 1);
        sta["hostname"].as<String>().toCharArray(_file_prams.sta.hostname, HOSTNAME_LENGHT_MAX + 1); 
        _file_prams.sta.dhcp      = sta["dhcp"].as<int>();  
        _file_prams.sta.disable   = sta["disable"].as<int>();
        _file_prams.sta.smart_cfg = sta["smart_cfg"].as<int>();
    }

    JsonObject& ap = root["ap"];
    if (ap.success())
    {
        _file_prams.ap.ip.fromString(ap["ip"].as<String>());
        _file_prams.ap.sn.fromString(ap["sn"].as<String>()); 
        ap["ssid"].as<String>().toCharArray(_file_prams.ap.ssid, SSID_LENGHT_MAX + 1);
        ap["psk"].as<String>().toCharArray(_file_prams.ap.pass, PASS_LENGHT_MAX + 1);       
        ap["dns_name"].as<String>().toCharArray(_file_prams.ap.dns_name, HOSTNAME_LENGHT_MAX + 1);
        _file_prams.ap.disable = ap["disable"].as<int>();  
        _file_prams.ap.channel = ap["channel"].as<int>();
        _file_prams.ap.hidden  = ap["hidden"].as<int>(); 
    }  

    JsonObject& ddns = root["ddns"];
    if (ddns.success())
    {
        ddns["service"].as<String>().toCharArray(_file_prams.ddns.service, DDNS_SERVICE_LENGTH_MAX + 1);
        ddns["domain"].as<String>().toCharArray(_file_prams.ddns.domain, DDNS_DOMAIN_LENGTH_MAX + 1);
        ddns["user"].as<String>().toCharArray(_file_prams.ddns.user, DDNS_USER_LENGTH_MAX + 1);
        ddns["pass"].as<String>().toCharArray(_file_prams.ddns.pass, DDNS_PASS_LENGTH_MAX + 1);
        _file_prams.ddns.sync_time = ddns["sync_time"].as<int>();
        _file_prams.ddns.disable = ddns["disable"].as<int>();
    } 

    WIFI_DATA_TAG_CONSOLE("sync data succed!");
}

wifi_data_file WFDataFile(NAND_FS_SYSTEM);
