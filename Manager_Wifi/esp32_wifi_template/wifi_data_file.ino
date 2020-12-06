#include <ArduinoJson.h>
#include "wifi_data_file.h"

#define WIFI_FILE_PORT Serial
#define WIFI_FILE_PRINTF(f_, ...) WIFI_FILE_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

// wifi json string default
const char wifi_file_default[] PROGMEM =
"{\"device_name\":\"THIẾT BỊ BÁN XĂNG BẰNG THẺ\","
"\n\"device_addr\":\"102 Phan Huy Ích, Phường 15, Quận Tân Bình, Tp HCM\"," 
"\n\"UDPPort\":25123,"	
"\n\"TCPPort\":25123,"
"\n\"WSPort\":25124,"
"\n\"AuthUser\":\"admin\","
"\n\"AuthPass\":\"admin\","
"\n\"SaticIp\":0,"
"\n\"Ip\":\"192.168.1.100\","
"\n\"Gw\":\"192.168.1.1\","
"\n\"Sn\":\"255.255.255.0\","
"\n\"Dns\":\"192.168.1.1\","
"\n\"STASsid\":\"Quyen_2.4G\","
"\n\"STAPass\":\"12345679\","
"\n\"STAName\":\"dtud\","
"\n\"APSsid\":\"MONTECH\","
"\n\"APPass\":\"88888888\","
"\n\"APName\":\"tienhuyiot\","
"\n\"APIp\":\"192.168.4.1\","
"\n\"APSn\":\"255.255.255.0\","
"\n\"Chanel\":2,"
"\n\"Hidden\":0}";

static wifi_file_json_t wifi_file_cfg;

void wifi_info_setup()
{
    wifi_info_read(&wifi_file_cfg);
}

wifi_file_json_t* wifi_info_get()
{
    return &wifi_file_cfg;
}

void wifi_info_refactor(void)
{
    NAND_FS_SYSTEM.remove(WIFI_FILE_PATH);
}

void wifi_info_write(wifi_file_json_t* w_wifi_info)
{
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();
    root["device_name"].set(w_wifi_info->addr.device_name);
    root["device_addr"].set(w_wifi_info->addr.device_addr);
    root["UDPPort"].set(w_wifi_info->UDPPort);
    root["TCPPort"].set(w_wifi_info->TCPPort);
    root["WSPort"].set(w_wifi_info->WSPort);
    root["AuthUser"].set(w_wifi_info->auth.user);
    root["AuthPass"].set(w_wifi_info->auth.pass);
    root["SaticIp"].set(w_wifi_info->sta.SaticIp);
    root["Ip"].set(w_wifi_info->sta.Ip.toString());
    root["Gw"].set(w_wifi_info->sta.Gw.toString());
    root["Sn"].set(w_wifi_info->sta.Sn.toString());
    root["Dns"].set(w_wifi_info->sta.Dns.toString());
    root["STASsid"].set(w_wifi_info->sta.ssid);
    root["STAPass"].set(w_wifi_info->sta.pass);
    root["STAName"].set(w_wifi_info->sta.HostName);
    root["APSsid"].set(w_wifi_info->ap.ssid);
    root["APPass"].set(w_wifi_info->ap.pass);
    root["APName"].set(w_wifi_info->ap.DnsName);
    root["APIp"].set(w_wifi_info->ap.Ip.toString());
    root["APSn"].set(w_wifi_info->ap.Sn.toString());
    root["Chanel"].set(w_wifi_info->ap.Chanel);
    root["Hidden"].set(w_wifi_info->ap.Hidden);    

    WIFI_FILE_PRINTF("\r\nJson created:\r\n");
    root.printTo(WIFI_FILE_PORT);
    fs_handle = NAND_FS_SYSTEM.open(WIFI_FILE_PATH, FILE_WRITE);
    root.prettyPrintTo(fs_handle);
    fs_handle.close();    
    WIFI_FILE_PRINTF("\r\nwifi info updated\r\n"); 
}

void wifi_info_read(wifi_file_json_t* r_wifi_info)
{
    // check file exist
    if (!NAND_FS_SYSTEM.exists(WIFI_FILE_PATH))
    {
        // write json string default
        fs_handle = NAND_FS_SYSTEM.open(WIFI_FILE_PATH, FILE_WRITE);
        fs_handle.printf_P(wifi_file_default);
        fs_handle.close();
    }

    fs_handle = NAND_FS_SYSTEM.open(WIFI_FILE_PATH, FILE_READ);
    size_t size = fs_handle.size();
    std::unique_ptr<char[]> buff(new char[size + 1]);
    fs_handle.readBytes(buff.get(), size);
    buff.get()[size] = 0;
    fs_handle.close();
    WIFI_FILE_PRINTF("\r\nwifi file content:\r\n%s\r\n", buff.get());

    DynamicJsonBuffer djbpo(size + 100);
    JsonObject &root = djbpo.parseObject(buff.get());
    if (!root.success())
    {
        WIFI_FILE_PRINTF("\r\nJSON parsing failed!\r\n");
        return;
    }
    r_wifi_info->UDPPort = root["UDPPort"].as<int>();
    r_wifi_info->TCPPort = root["TCPPort"].as<int>();
    r_wifi_info->WSPort = root["WSPort"].as<int>();
    r_wifi_info->sta.SaticIp = root["SaticIp"].as<int>();
    r_wifi_info->ap.Chanel = root["Chanel"].as<int>();
    r_wifi_info->ap.Hidden = root["Hidden"].as<int>();
    r_wifi_info->sta.Ip.fromString(root["Ip"].as<String>());
    r_wifi_info->sta.Gw.fromString(root["Gw"].as<String>());
    r_wifi_info->sta.Sn.fromString(root["Sn"].as<String>());
    r_wifi_info->sta.Dns.fromString(root["Dns"].as<String>());
    r_wifi_info->ap.Ip.fromString(root["APIp"].as<String>());
    r_wifi_info->ap.Sn.fromString(root["APSn"].as<String>()); 
    root["device_name"].as<String>().toCharArray(r_wifi_info->addr.device_name, Df_LengAddr + 1);
    root["device_addr"].as<String>().toCharArray(r_wifi_info->addr.device_addr, Df_LengAddr + 1);    
    root["AuthUser"].as<String>().toCharArray(r_wifi_info->auth.user, Df_LengAuth + 1);
    root["AuthPass"].as<String>().toCharArray(r_wifi_info->auth.pass, Df_LengAuth + 1);
    root["STASsid"].as<String>().toCharArray(r_wifi_info->sta.ssid, Df_LengSsid + 1);
    root["STAPass"].as<String>().toCharArray(r_wifi_info->sta.pass, Df_LengPass + 1);
    root["APSsid"].as<String>().toCharArray(r_wifi_info->ap.ssid, Df_LengSsid + 1);
    root["APPass"].as<String>().toCharArray(r_wifi_info->ap.pass, Df_LengPass + 1);
    root["STAName"].as<String>().toCharArray(r_wifi_info->sta.HostName, Df_LengHostName + 1);    
    root["APName"].as<String>().toCharArray(r_wifi_info->ap.DnsName, Df_LengHostName + 1);       
}
