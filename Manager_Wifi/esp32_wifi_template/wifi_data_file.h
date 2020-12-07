#ifndef	_WIFI_DATA_FILE_
#define _WIFI_DATA_FILE_

#define WIFI_FILE_PATH "/wifi_info_file.txt"

#define Df_LengHostName       32
#define Df_LengSsid           32
#define Df_LengPass           64
#define Df_LengAuth           10
#define Df_LengDevName        100
#define Df_LengAddr           100
#define Df_LengServer         32

typedef struct {
    uint16_t 		UDPPort;      /*UDP Port*/
    uint16_t 		TCPPort;      /*TCP Port*/
    uint16_t        WSPort;       /*Websocket Port*/
    struct {
        char        device_name[Df_LengDevName + 1];
        char        device_addr[Df_LengAddr + 1];
    }addr;
    struct {
        char        user[Df_LengAuth + 1];
        char        pass[Df_LengAuth + 1];
    }auth;
    struct {
        IPAddress 	Ip;
        IPAddress 	Gw;
        IPAddress 	Sn;
        IPAddress 	Dns;
        char 		ssid[Df_LengSsid + 1];
        char 		pass[Df_LengPass + 1];
        char        HostName[Df_LengHostName+1];                
        uint8_t 	Dhcp    : 1;
        uint8_t     Dis     : 1;
        uint8_t     SmCfg   : 1; /* SmartConfig */
    }sta;
    struct {
        IPAddress   Ip;
        IPAddress   Sn; 
        char 		ssid[Df_LengSsid + 1];
        char 		pass[Df_LengPass + 1];
        char        DnsName[Df_LengHostName+1];               
        uint8_t 	Chanel : 4;
        uint8_t 	Hidden : 1;
        uint8_t     Dis    : 1;
    }ap;
} wifi_file_json_t;

wifi_file_json_t wifi_file_cfg;

void wifi_info_setup(void);
wifi_file_json_t* wifi_info_get(void);
void wifi_info_refactor(void);
void wifi_info_read(wifi_file_json_t* r_wifi_info);
void wifi_info_write(wifi_file_json_t* w_wifi_info);

#endif