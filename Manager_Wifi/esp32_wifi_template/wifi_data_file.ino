#include <ArduinoJson.h>
#include "wifi_data_file.h"

#define WIFI_FILE_PORT Serial
#define WIFI_FILE_PRINTF(f_, ...) WIFI_FILE_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void wifi_info_setup()
{
    wifi_info_read(&wifi_file_cfg);
}

wifi_file_json_t* wifi_info_get()
{
    return &wifi_file_cfg;
}

void wifi_info_pass_refactor(void)
{
    strncpy(wifi_file_cfg.auth.pass, (char *)"admin", Df_LengAuth);
    strncpy(wifi_file_cfg.auth_user.pass, (char *)"admin", Df_LengAuth);
    wifi_file_cfg.confirm[CONFIRM_COMMON] = PASS_COMMON_DEFAULT;
    wifi_file_cfg.confirm[CONFIRM_PU3] = PASS_PU3_DEFAULT;
    wifi_file_cfg.confirm[CONFIRM_COST] = PASS_COST_DEFAULT;
    wifi_file_cfg.confirm[CONFIRM_LOGFILE] = PASS_LOGFILE_DEFAULT;
    wifi_info_write(&wifi_file_cfg);
}

uint8_t pass_superadmin_is_ok(const String &pass)
{
    return (pass == "25251325");
}

uint8_t pass_type_is_ok(const String &pass, uint8_t type)
{
    uint8_t result = 0;
    if(pass == String(wifi_file_cfg.confirm[type]) || pass_superadmin_is_ok(pass))
    {
        result = 1;
    }
    return result;
}

void wifi_info_refactor(void)
{
    NAND_FS_SYSTEM.remove(WIFI_FILE_PATH);
}

void wifi_info_write(wifi_file_json_t* w_wifi_info)
{
    File fs_handle;
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();

    JsonObject& port = root.createNestedObject("port");
    port["udp"].set(w_wifi_info->port.udp);
    port["tcp"].set(w_wifi_info->port.tcp);
    port["ws"].set(w_wifi_info->port.ws);

    JsonObject& device = root.createNestedObject("device");
    device["name"].set(w_wifi_info->device.name);
    device["addr"].set(w_wifi_info->device.addr);
    device["tell"].set(w_wifi_info->device.tell);

    JsonObject& auth = root.createNestedObject("auth");
    auth["user"].set(w_wifi_info->auth.user);
    auth["pass"].set(w_wifi_info->auth.pass);

    JsonObject& auth_user = root.createNestedObject("auth_user");
    auth_user["user"].set(w_wifi_info->auth_user.user);
    auth_user["pass"].set(w_wifi_info->auth_user.pass);

    JsonArray& confirm = root.createNestedArray("confirm");
    for(uint8_t i = 0; i < CONFIRM_NUM_MAX; ++i)
    {
        confirm.add(w_wifi_info->confirm[i]);
    }

    JsonObject& sta = root.createNestedObject("sta");
    sta["ip"].set(w_wifi_info->sta.ip.toString());
    sta["gw"].set(w_wifi_info->sta.gw.toString());
    sta["sn"].set(w_wifi_info->sta.sn.toString());
    sta["dns"].set(w_wifi_info->sta.dns.toString());
    sta["ssid"].set(w_wifi_info->sta.ssid);
    sta["psk"].set(w_wifi_info->sta.pass);
    sta["hostname"].set(w_wifi_info->sta.hostname);
    sta["dhcp"].set(w_wifi_info->sta.dhcp);
    sta["disable"].set(w_wifi_info->sta.disable);
    sta["smart_cfg"].set(w_wifi_info->sta.smart_cfg);

    JsonObject& ap = root.createNestedObject("ap");
    ap["ip"].set(w_wifi_info->ap.ip.toString());
    ap["sn"].set(w_wifi_info->ap.sn.toString());
    ap["ssid"].set(w_wifi_info->ap.ssid);
    ap["psk"].set(w_wifi_info->ap.pass);
    ap["dns_name"].set(w_wifi_info->ap.dns_name);    
    ap["disable"].set(w_wifi_info->ap.disable);
    ap["hidden"].set(w_wifi_info->ap.hidden);
    ap["channel"].set(w_wifi_info->ap.channel); 

    JsonObject& ddns = root.createNestedObject("ddns");
    ddns["service"].set(w_wifi_info->ddns.service);
    ddns["domain"].set(w_wifi_info->ddns.domain);
    ddns["user"].set(w_wifi_info->ddns.user);    
    ddns["pass"].set(w_wifi_info->ddns.pass); 
    ddns["sync_time"].set(w_wifi_info->ddns.sync_time); 
    ddns["disable"].set(w_wifi_info->ddns.disable);

    WIFI_FILE_PRINTF("\r\nJson created:\r\n");
    // root.prettyPrintTo(WIFI_FILE_PORT);
    fs_handle = NAND_FS_SYSTEM.open(WIFI_FILE_PATH, FILE_WRITE);
    root.prettyPrintTo(fs_handle);
    fs_handle.close();    
    WIFI_FILE_PRINTF("\r\nwifi json info updated\r\n"); 
}

void wifi_info_read(wifi_file_json_t* r_wifi_info)
{
    File fs_handle;

    if (!NAND_FS_SYSTEM.exists(WIFI_FILE_PATH))
    {
        fs_handle = NAND_FS_SYSTEM.open(WIFI_FILE_PATH, FILE_WRITE);
        fs_handle.printf_P(wifi_data_json);
        // WIFI_FILE_PORT.println(wifi_data_json);
        fs_handle.close();
    }

    fs_handle = NAND_FS_SYSTEM.open(WIFI_FILE_PATH, FILE_READ);    
    
    DynamicJsonBuffer djbpo;
    JsonObject &root = djbpo.parseObject(fs_handle);
    fs_handle.close();
    
    if (!root.success())
    {
        WIFI_FILE_PRINTF("\r\nJSON parsing failed!\r\n");
        return;
    }

    JsonObject& port = root["port"];
    if (port.success())
    {
        r_wifi_info->port.udp = port["udp"].as<int>();
        r_wifi_info->port.tcp = port["tcp"].as<int>();
        r_wifi_info->port.ws  = port["ws"].as<int>();
    }

    JsonObject& device = root["device"];
    if (device.success())
    {
        device["name"].as<String>().toCharArray(r_wifi_info->device.name, Df_LengDevName + 1);
        device["addr"].as<String>().toCharArray(r_wifi_info->device.addr, Df_LengAddr + 1);
        device["tell"].as<String>().toCharArray(r_wifi_info->device.tell, df_LengTell + 1);
    }

    JsonObject& auth = root["auth"];
    if (auth.success())
    {
        auth["user"].as<String>().toCharArray(r_wifi_info->auth.user, Df_LengAuth + 1);
        auth["pass"].as<String>().toCharArray(r_wifi_info->auth.pass, Df_LengAuth + 1);
    }

    JsonObject& auth_user = root["auth_user"];
    if (auth_user.success())
    {
        auth_user["user"].as<String>().toCharArray(r_wifi_info->auth_user.user, Df_LengAuth + 1);
        auth_user["pass"].as<String>().toCharArray(r_wifi_info->auth_user.pass, Df_LengAuth + 1);
    }

    JsonArray& confirm = root["confirm"];
    if (confirm.success())
    {
        uint8_t num = min((int)(confirm.size()), CONFIRM_NUM_MAX);
        for(uint8_t i = 0; i < num; ++i)
        {
            r_wifi_info->confirm[i] = confirm[i];
        }        
    }

    JsonObject& sta = root["sta"];
    if (sta.success())
    {
        r_wifi_info->sta.ip.fromString(sta["ip"].as<String>());
        r_wifi_info->sta.gw.fromString(sta["gw"].as<String>());
        r_wifi_info->sta.sn.fromString(sta["sn"].as<String>());
        r_wifi_info->sta.dns.fromString(sta["dns"].as<String>());
        sta["ssid"].as<String>().toCharArray(r_wifi_info->sta.ssid, Df_LengSsid + 1);
        sta["psk"].as<String>().toCharArray(r_wifi_info->sta.pass, Df_LengPass + 1);
        sta["hostname"].as<String>().toCharArray(r_wifi_info->sta.hostname, Df_LengHostName + 1); 
        r_wifi_info->sta.dhcp      = sta["dhcp"].as<int>();  
        r_wifi_info->sta.disable   = sta["disable"].as<int>();
        r_wifi_info->sta.smart_cfg = sta["smart_cfg"].as<int>();
    }

    JsonObject& ap = root["ap"];
    if (ap.success())
    {
        r_wifi_info->ap.ip.fromString(ap["ip"].as<String>());
        r_wifi_info->ap.sn.fromString(ap["sn"].as<String>()); 
        ap["ssid"].as<String>().toCharArray(r_wifi_info->ap.ssid, Df_LengSsid + 1);
        ap["psk"].as<String>().toCharArray(r_wifi_info->ap.pass, Df_LengPass + 1);       
        ap["dns_name"].as<String>().toCharArray(r_wifi_info->ap.dns_name, Df_LengHostName + 1);
        r_wifi_info->ap.disable = ap["disable"].as<int>();  
        r_wifi_info->ap.channel = ap["channel"].as<int>();
        r_wifi_info->ap.hidden  = ap["hidden"].as<int>(); 
    }  

    JsonObject& ddns = root["ddns"];
    if (ddns.success())
    {
        ddns["service"].as<String>().toCharArray(r_wifi_info->ddns.service, DDNS_SERVICE_LENGTH_MAX + 1);
        ddns["domain"].as<String>().toCharArray(r_wifi_info->ddns.domain, DDNS_DOMAIN_LENGTH_MAX + 1);
        ddns["user"].as<String>().toCharArray(r_wifi_info->ddns.user, DDNS_USER_LENGTH_MAX + 1);
        ddns["pass"].as<String>().toCharArray(r_wifi_info->ddns.pass, DDNS_PASS_LENGTH_MAX + 1);
        r_wifi_info->ddns.sync_time = ddns["sync_time"].as<int>();
        r_wifi_info->ddns.disable = ddns["disable"].as<int>();
    } 
}
