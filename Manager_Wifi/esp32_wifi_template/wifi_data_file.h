/*
Check json online: http://json.parser.online.fr/
Json arduino assistant: https://arduinojson.org/v5/assistant/
*/

#ifndef	_WIFI_DATA_FILE_
#define _WIFI_DATA_FILE_

#define WIFI_FILE_PATH "/wifi_info_file.txt"

#define Df_LengHostName       32
#define Df_LengSsid           32
#define Df_LengPass           64
#define Df_LengAuth           10
#define Df_LengDevName        127
#define Df_LengAddr           127
#define df_LengTell           15
#define Df_LengServer         32
#define DDNS_SERVICE_LENGTH_MAX 15
#define DDNS_DOMAIN_LENGTH_MAX  31
#define DDNS_USER_LENGTH_MAX    15
#define DDNS_PASS_LENGTH_MAX    15
#define CONFIRM_NUM_MAX         5

#define PASS_COMMON_DEFAULT     1234
#define PASS_PU3_DEFAULT        1234
#define PASS_COST_DEFAULT       1234
#define PASS_LOGFILE_DEFAULT    1234
typedef enum {
    CONFIRM_COMMON = 0,
    CONFIRM_PU3,
    CONFIRM_COST,
    CONFIRM_LOGFILE,
    CONFIRM_RESERVE
} confirm_t;

typedef struct {
    struct {
        uint16_t 	udp;
        uint16_t 	tcp;
        uint16_t    ws;
    }port;    
    struct {
        char        name[Df_LengDevName + 1];
        char        addr[Df_LengAddr + 1];
        char        tell[df_LengTell + 1];
    }device;
    struct {
        char        user[Df_LengAuth + 1];
        char        pass[Df_LengAuth + 1];
    }auth;
    struct {
        char        user[Df_LengAuth + 1];
        char        pass[Df_LengAuth + 1];
    }auth_user;
    
    uint16_t confirm[CONFIRM_NUM_MAX];

    struct {
        IPAddress 	ip;
        IPAddress 	gw;
        IPAddress 	sn;
        IPAddress 	dns;
        char 		ssid[Df_LengSsid + 1];
        char 		pass[Df_LengPass + 1];
        char        hostname[Df_LengHostName+1];                
        uint8_t 	dhcp        : 1;
        uint8_t     disable     : 1;
        uint8_t     smart_cfg   : 1;
    }sta;
    struct {
        IPAddress   ip;
        IPAddress   sn; 
        char 		ssid[Df_LengSsid + 1];
        char 		pass[Df_LengPass + 1];
        char        dns_name[Df_LengHostName+1];               
        uint8_t 	channel : 4;
        uint8_t 	hidden  : 1;
        uint8_t     disable : 1;
    }ap;
    struct {
        char        service[DDNS_SERVICE_LENGTH_MAX + 1];
        char        domain[DDNS_DOMAIN_LENGTH_MAX + 1];
        char        user[DDNS_USER_LENGTH_MAX + 1];
        char        pass[DDNS_PASS_LENGTH_MAX + 1];
        uint8_t     sync_time;
        uint8_t     disable : 1;
    }ddns;
} wifi_file_json_t;

const char wifi_data_json[] PROGMEM = R"=====(
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

wifi_file_json_t wifi_file_cfg;

void wifi_info_setup(void);
wifi_file_json_t* wifi_info_get(void);
void wifi_info_refactor(void);
void wifi_info_pass_refactor(void);
uint8_t pass_superadmin_is_ok(const String &pass);
uint8_t pass_type_is_ok(const String &pass, uint8_t type);
void wifi_info_read(wifi_file_json_t* r_wifi_info);
void wifi_info_write(wifi_file_json_t* w_wifi_info);

#endif