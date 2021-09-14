#ifndef	__HTH_ESP_SYS_DATA_H
#define __HTH_ESP_SYS_DATA_H

#include <Arduino.h>
#include <FS.h>

#define WIFI_FILE_PATH ((const char*)"/wifi_info_file.txt")

#define HOSTNAME_LENGHT_MAX     32
#define SSID_LENGHT_MAX         32
#define PASS_LENGHT_MAX         64
#define AUTH_LENGHT_MAX         10
#define DEVICENAME_LENGHT_MAX   127
#define DEVICE_ADDR_LENGHT_MAX  127
#define DEVICE_TELL_LENGHT_MAX  15
#define DDNS_SERVICE_LENGTH_MAX 15
#define DDNS_DOMAIN_LENGTH_MAX  31
#define DDNS_USER_LENGTH_MAX    15
#define DDNS_PASS_LENGTH_MAX    15

#define PASS_COMMON_DEFAULT     1234
#define PASS_PU3_DEFAULT        1234
#define PASS_COST_DEFAULT       1234
#define PASS_LOGFILE_DEFAULT    1234

typedef enum : uint8_t {
    CONFIRM_COMMON = 0,
    CONFIRM_PU3,
    CONFIRM_COST,
    CONFIRM_LOGFILE,
    CONFIRM_RESERVE,
    CONFIRM_NUM_MAX
} wifi_pass_confirm_t;

typedef struct {
    struct {
        uint16_t 	udp;
        uint16_t 	tcp;
        uint16_t    ws;
    }port;    
    struct {
        char        name[DEVICENAME_LENGHT_MAX + 1];
        char        addr[DEVICE_ADDR_LENGHT_MAX + 1];
        char        tell[DEVICE_TELL_LENGHT_MAX + 1];
    }device;
    struct {
        char        user[AUTH_LENGHT_MAX + 1];
        char        pass[AUTH_LENGHT_MAX + 1];
    }auth_admin;
    struct {
        char        user[AUTH_LENGHT_MAX + 1];
        char        pass[AUTH_LENGHT_MAX + 1];
    }auth_user;
    
    uint16_t confirm[CONFIRM_NUM_MAX];

    struct {
        IPAddress 	ip;
        IPAddress 	gw;
        IPAddress 	sn;
        IPAddress 	dns;
        char 		ssid[SSID_LENGHT_MAX + 1];
        char 		pass[PASS_LENGHT_MAX + 1];
        char        hostname[HOSTNAME_LENGHT_MAX + 1];                
        uint8_t 	dhcp        : 1;
        uint8_t     disable     : 1;
        uint8_t     smart_cfg   : 1;
    }sta;
    struct {
        IPAddress   ip;
        IPAddress   sn; 
        char 		ssid[SSID_LENGHT_MAX + 1];
        char 		pass[PASS_LENGHT_MAX + 1];
        char        dns_name[HOSTNAME_LENGHT_MAX + 1];               
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

class wifi_data_file
{
private:
    fs::FS *_fs;
    wifi_file_json_t _file_prams;
    void syncFromFileSystem();
    void saveToFileSystem();
public:
    wifi_data_file(fs::FS &fs);
    ~wifi_data_file();

    void begin();
    void commitToFS() { saveToFileSystem(); }
    void resetPassword();
    void resetDefault();
    bool passSupperAdminIsOK(const String &pass);
    bool passConfirmIsOK(const String &pass, wifi_pass_confirm_t type);
    /* Port API */
    uint16_t udpPort() const { return _file_prams.port.udp; }
    uint16_t tcpPort() { return _file_prams.port.tcp; }
    uint16_t wsPort() { return _file_prams.port.ws; }
    void udpPortSet(uint16_t port) { _file_prams.port.udp = port; }
    void tcpPortSet(uint16_t port) { _file_prams.port.tcp = port; }
    void wsPortSet(uint16_t port) { _file_prams.port.ws = port; }
    /* Device API */
    String nameDevice() { return _file_prams.device.name; }
    String addrDevice() { return _file_prams.device.addr; }
    String tellDevice() { return _file_prams.device.tell; }
    void nameDeviceSet(const String &name) { 
        name.toCharArray(_file_prams.device.name, DEVICENAME_LENGHT_MAX + 1);
    }
    void addrDeviceSet(const String &addr) { 
        addr.toCharArray(_file_prams.device.addr, DEVICE_ADDR_LENGHT_MAX + 1);
    }
    void tellDeviceSet(const String &tell) { 
        tell.toCharArray(_file_prams.device.tell, DEVICE_TELL_LENGHT_MAX + 1);
    }
    /* Auth API */
    String authAdminUser() { return _file_prams.auth_admin.user; }
    String authAdminPass() { return _file_prams.auth_admin.pass; }
    String authUserUser() { return _file_prams.auth_user.user; }
    String authUserPass() { return _file_prams.auth_user.pass; }
    uint16_t passConfirm(wifi_pass_confirm_t passType) { 
        return _file_prams.confirm[passType]; 
    }
    void authAdminUserSet(const String &user) { 
        user.toCharArray(_file_prams.auth_admin.user, AUTH_LENGHT_MAX + 1);
    }
    void authAdminPassSet(const String &pass) { 
        pass.toCharArray(_file_prams.auth_admin.pass, AUTH_LENGHT_MAX + 1);
    }
    void authUserUserSet(const String &user) { 
        user.toCharArray(_file_prams.auth_user.user, AUTH_LENGHT_MAX + 1);
    }
    void authUserPassSet(const String &pass) { 
        pass.toCharArray(_file_prams.auth_user.pass, AUTH_LENGHT_MAX + 1);
    }
    void passConfirmSet(wifi_pass_confirm_t passType, uint16_t pass) { 
        _file_prams.confirm[passType] = pass;
    }
    /* STA API */
    IPAddress ipSTA() { return _file_prams.sta.ip; }
    IPAddress gwSTA() { return _file_prams.sta.gw; }
    IPAddress snSTA() { return _file_prams.sta.sn; }
    IPAddress dnsSTA() { return _file_prams.sta.dns; }
    String ssidSTA() { return _file_prams.sta.ssid; }
    String passSTA() { return _file_prams.sta.pass; }
    String hostNameSTA() { return _file_prams.sta.hostname; }
    bool dhcpSTA() { return _file_prams.sta.dhcp; }
    bool isDisableSTA() { return _file_prams.sta.disable; }
    bool smartCfgSTA() { return _file_prams.sta.smart_cfg; }
    void ipSTASet(IPAddress ip) { _file_prams.sta.ip = ip; }
    void gwSTASet(IPAddress gw) { _file_prams.sta.gw = gw; }
    void snSTASet(IPAddress sn) { _file_prams.sta.sn = sn; }
    void dnsSTASet(IPAddress dns) { _file_prams.sta.dns = dns; }
    void ssidSTASet(const String &ssid) { 
        ssid.toCharArray(_file_prams.sta.ssid, SSID_LENGHT_MAX + 1);
    }
    void passSTASet(const String &pass) { 
        pass.toCharArray(_file_prams.sta.pass, PASS_LENGHT_MAX + 1);
    }
    void hostNameSTASet(const String &hostName) { 
        hostName.toCharArray(_file_prams.sta.hostname, HOSTNAME_LENGHT_MAX + 1);
    }
    void dhcpSTASet(bool dhcp) { _file_prams.sta.dhcp = dhcp; }
    void disableSTASet(bool disable) { _file_prams.sta.disable = disable; }
    void smartCfgSTASet(bool smart_cfg) { _file_prams.sta.smart_cfg = smart_cfg; }
    /* AP API */
    IPAddress ipAP() { return _file_prams.ap.ip; }
    IPAddress snAP() { return _file_prams.ap.sn; }
    String ssidAP() { return _file_prams.ap.ssid; }
    String passAP() { return _file_prams.ap.pass; }
    String dnsNameAP() { return _file_prams.ap.dns_name; }
    bool channelAP() { return _file_prams.ap.channel; }
    bool isHiddenAP() { return _file_prams.ap.hidden; }
    bool isDisableAP() { return _file_prams.ap.disable; }
    void ipAPSet(IPAddress ip) { _file_prams.ap.ip = ip; }
    void snAPSet(IPAddress sn) { _file_prams.ap.sn = sn; }
    void ssidAPSet(const String &ssid) { 
        ssid.toCharArray(_file_prams.ap.ssid, SSID_LENGHT_MAX + 1);
    }
    void passAPSet(const String &pass) { 
        pass.toCharArray(_file_prams.ap.pass, PASS_LENGHT_MAX + 1);
    }
    void dnsNameAPSet(const String &dnsName) { 
        dnsName.toCharArray(_file_prams.ap.dns_name, HOSTNAME_LENGHT_MAX + 1);
    }
    void channelAPSet(bool channel) { _file_prams.ap.channel = channel; }
    void hiddenAPSet(bool hidden) { _file_prams.ap.hidden = hidden; }
    void disableAPSet(bool disable) { _file_prams.ap.disable = disable; }
    /* DDNS API */
    String serviceDDNS() { return _file_prams.ddns.service; }
    String domainDDNS() { return _file_prams.ddns.domain; }
    String userDDNS() { return _file_prams.ddns.user; }
    String passDDNS() { return _file_prams.ddns.pass; }
    uint8_t syncTimeDDNS() { return _file_prams.ddns.sync_time; }
    bool disableDDNS() { return _file_prams.ddns.disable; }
    void serviceDDNSSet(const String &service) { 
        service.toCharArray(_file_prams.ddns.service, DDNS_SERVICE_LENGTH_MAX + 1);
    }
    void domainDDNSSet(const String &domain) { 
        domain.toCharArray(_file_prams.ddns.domain, DDNS_DOMAIN_LENGTH_MAX + 1);
    }
    void userDDNSSet(const String &user) { 
        user.toCharArray(_file_prams.ddns.user, DDNS_USER_LENGTH_MAX + 1);
    }
    void passDDNSSet(const String &pass) { 
        pass.toCharArray(_file_prams.ddns.pass, DDNS_PASS_LENGTH_MAX + 1);
    }
    void syncTimeDDNSSet(uint8_t sync_time) { _file_prams.ddns.sync_time = sync_time; }
    void disableDDNSSet(bool disable) { _file_prams.ddns.disable = disable; }
};

extern wifi_data_file WFDataFile;

#endif // __HTH_ESP_SYS_DATA_H
