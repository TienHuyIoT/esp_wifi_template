/*
Check json online: http://json.parser.online.fr/
Json arduino assistant: https://arduinojson.org/v5/assistant/
*/

#ifndef	__ESP_SYS_PARAMS_H
#define __ESP_SYS_PARAMS_H

#include <Arduino.h>
#include <FS.h>

#define ESP_SYSTEM_PARAMS ((const char*)"/esp_system_params.txt")

#define HOSTNAME_LENGTH_MAX     32
#define SSID_LENGTH_MAX         32
#define PASS_LENGTH_MAX         64
#define AUTH_LENGTH_MAX         10
#define DEVICE_NAME_LENGTH_MAX  127
#define DEVICE_ADDR_LENGTH_MAX  127
#define DEVICE_TELL_LENGTH_MAX  15
#define DDNS_SERVICE_LENGTH_MAX 15
#define DDNS_DOMAIN_LENGTH_MAX  31
#define DDNS_USER_LENGTH_MAX    15
#define DDNS_PASS_LENGTH_MAX    15
#define DDNS_IPURL_LENGTH_MAX   50
#define SNTP_SERVER_LENGTH_MAX  31
#define SNTP_TZ_LENGTH_MAX      40
#define PING_HOST_LENGTH_MAX    31

#define PING_HOST1_INDEX        0
#define PING_HOST2_INDEX        1
#define PING_HOST_NUM           2

#define PASS_COMMON_DEFAULT     1234
#define PASS_EX1_DEFAULT        1234
#define PASS_EX2_DEFAULT        1234
#define PASS_EX3_DEFAULT        1234
#define PASS_EX4_DEFAULT        1234
#define CONFIRM_NUM_MAX         5

typedef struct {
    struct {
        uint16_t 	udp;    /* Udp port, reserved */
        uint16_t 	tcp;    /* tcp port for webServer */
        uint16_t    ws;     /* websocket port, not used */
    }port;    
    struct {
        char        name[DEVICE_NAME_LENGTH_MAX + 1];
        char        addr[DEVICE_ADDR_LENGTH_MAX + 1];
        char        tell[DEVICE_TELL_LENGTH_MAX + 1];
    }device;
    struct {
        char        user[AUTH_LENGTH_MAX + 1];
        char        pass[AUTH_LENGTH_MAX + 1];
    }auth_admin;
    struct {
        char        user[AUTH_LENGTH_MAX + 1];
        char        pass[AUTH_LENGTH_MAX + 1];
    }auth_user;
    
    uint16_t confirm[CONFIRM_NUM_MAX];

    struct {
        IPAddress 	ip;
        IPAddress 	gw;
        IPAddress 	sn;
        IPAddress 	dns;
        char 		ssid[SSID_LENGTH_MAX + 1];
        char 		pass[PASS_LENGTH_MAX + 1];
        char        hostname[HOSTNAME_LENGTH_MAX + 1];                
        uint8_t 	dhcp        : 1;
        uint8_t     disable     : 1;
        uint8_t     smart_cfg   : 1;
    }sta;
    struct {
        IPAddress   ip;
        IPAddress   sn; 
        char 		ssid[SSID_LENGTH_MAX + 1];
        char 		pass[PASS_LENGTH_MAX + 1];
        char        dns_name[HOSTNAME_LENGTH_MAX + 1];               
        uint8_t 	channel : 4;
        uint8_t 	hidden  : 1;
        uint8_t     disable : 1;
    }ap;
    struct {
        char        service[DDNS_SERVICE_LENGTH_MAX + 1];
        char        domain[DDNS_DOMAIN_LENGTH_MAX + 1];
        char        user[DDNS_USER_LENGTH_MAX + 1];
        char        pass[DDNS_PASS_LENGTH_MAX + 1];
        char        ipUrl[DDNS_IPURL_LENGTH_MAX + 1];
        uint8_t     sync_time;
        uint8_t     disable : 1;
    }ddns;
    struct {
        char        server1[SNTP_SERVER_LENGTH_MAX + 1];
        char        server2[SNTP_SERVER_LENGTH_MAX + 1];
        char        server3[SNTP_SERVER_LENGTH_MAX + 1];
        char        TzTime[SNTP_TZ_LENGTH_MAX + 1];
        long        gmtOffset;
        int         daylightOffset;
        int         interval;
    }sntp;
    struct {
        char        host[PING_HOST_NUM][PING_HOST_LENGTH_MAX + 1];
        int         timeout[PING_HOST_NUM]; /*<! timeout to reset if ping error*/
        int         interval[PING_HOST_NUM];/*<! interval ping */
        int         type[PING_HOST_NUM];    /*<! 0: None, 1: reset enable*/
        bool        enable[PING_HOST_NUM];  /*<! 1: enable, 0: disable*/
    }ping;
} esp_sys_params_t;

class ESPSysParams
{
    using File = fs::File;
    using FS = fs::FS;
public:
    typedef enum : uint8_t {
        CONFIRM_COMMON = 0,
        CONFIRM_EX1,
        CONFIRM_EX2,
        CONFIRM_EX3,
        CONFIRM_EX4
    } passConfirm_t;

private:
    FS *_fs;
    esp_sys_params_t _sys_prams;
    void syncFromFileSystem();
    void saveToFileSystem();
public:
    ESPSysParams(FS &fs);
    ~ESPSysParams();

    void load(FS *fs = nullptr);
    void save() { saveToFileSystem(); }
    void setDefault() { _fs->remove(ESP_SYSTEM_PARAMS); }
    void resetPassword();
    bool passSupperAdminIsOK(const String &pass);
    /* Port API */
    uint16_t udpPort() const { return _sys_prams.port.udp; }
    uint16_t tcpPort() { return _sys_prams.port.tcp; }
    uint16_t wsPort() { return _sys_prams.port.ws; }
    void udpPortSet(uint16_t port) { _sys_prams.port.udp = port; }
    void tcpPortSet(uint16_t port) { _sys_prams.port.tcp = port; }
    void wsPortSet(uint16_t port) { _sys_prams.port.ws = port; }
    /* Device API */
    String nameDevice() { return _sys_prams.device.name; }
    String addrDevice() { return _sys_prams.device.addr; }
    String tellDevice() { return _sys_prams.device.tell; }
    void nameDeviceSet(const String &name) { 
        name.toCharArray(_sys_prams.device.name, DEVICE_NAME_LENGTH_MAX + 1);
    }
    void addrDeviceSet(const String &addr) { 
        addr.toCharArray(_sys_prams.device.addr, DEVICE_ADDR_LENGTH_MAX + 1);
    }
    void tellDeviceSet(const String &tell) { 
        tell.toCharArray(_sys_prams.device.tell, DEVICE_TELL_LENGTH_MAX + 1);
    }
    /* Auth API */
    String authAdminUser() { return _sys_prams.auth_admin.user; }
    String authAdminPass() { return _sys_prams.auth_admin.pass; }
    String authUserUser() { return _sys_prams.auth_user.user; }
    String authUserPass() { return _sys_prams.auth_user.pass; }
    void authAdminUserSet(const String &user) { 
        user.toCharArray(_sys_prams.auth_admin.user, AUTH_LENGTH_MAX + 1);
    }
    void authAdminPassSet(const String &pass) { 
        pass.toCharArray(_sys_prams.auth_admin.pass, AUTH_LENGTH_MAX + 1);
    }
    void authUserUserSet(const String &user) { 
        user.toCharArray(_sys_prams.auth_user.user, AUTH_LENGTH_MAX + 1);
    }
    void authUserPassSet(const String &pass) { 
        pass.toCharArray(_sys_prams.auth_user.pass, AUTH_LENGTH_MAX + 1);
    }
    /* Confirm pass */
    bool passConfirmIsOK(const String &pass, passConfirm_t type);
    uint16_t passConfirm(passConfirm_t passType) { 
        return _sys_prams.confirm[passType]; 
    }
    void passConfirmSet(passConfirm_t passType, uint16_t pass) { 
        _sys_prams.confirm[passType] = pass;
    }
    /* STA API */
    IPAddress ipSTA() { return _sys_prams.sta.ip; }
    IPAddress gwSTA() { return _sys_prams.sta.gw; }
    IPAddress snSTA() { return _sys_prams.sta.sn; }
    IPAddress dnsSTA() { return _sys_prams.sta.dns; }
    String ssidSTA() { return _sys_prams.sta.ssid; }
    String passSTA() { return _sys_prams.sta.pass; }
    String hostNameSTA() { return _sys_prams.sta.hostname; }
    uint8_t dhcpSTA() { return _sys_prams.sta.dhcp; }
    uint8_t isDisableSTA() { return _sys_prams.sta.disable; }
    uint8_t smartCfgSTA() { return _sys_prams.sta.smart_cfg; }
    void ipSTASet(IPAddress ip) { _sys_prams.sta.ip = ip; }
    void ipSTASet(const String &address) { _sys_prams.sta.ip.fromString(address); }
    void gwSTASet(IPAddress gw) { _sys_prams.sta.gw = gw; }
    void gwSTASet(const String &address) { _sys_prams.sta.gw.fromString(address); }
    void snSTASet(IPAddress sn) { _sys_prams.sta.sn = sn; }
    void snSTASet(const String &address) { _sys_prams.sta.sn.fromString(address); }
    void dnsSTASet(IPAddress dns) { _sys_prams.sta.dns = dns; }
    void dnsSTASet(const String &address) { _sys_prams.sta.dns.fromString(address); }
    void ssidSTASet(const String &ssid) { 
        ssid.toCharArray(_sys_prams.sta.ssid, SSID_LENGTH_MAX + 1);
    }
    void passSTASet(const String &pass) { 
        pass.toCharArray(_sys_prams.sta.pass, PASS_LENGTH_MAX + 1);
    }
    void hostNameSTASet(const String &hostName) { 
        hostName.toCharArray(_sys_prams.sta.hostname, HOSTNAME_LENGTH_MAX + 1);
    }
    void dhcpSTASet(uint8_t dhcp) { _sys_prams.sta.dhcp = dhcp; }
    void disableSTASet(uint8_t disable) { _sys_prams.sta.disable = disable; }
    void smartCfgSTASet(uint8_t smart_cfg) { _sys_prams.sta.smart_cfg = smart_cfg; }
    /* AP API */
    IPAddress ipAP() { return _sys_prams.ap.ip; }
    IPAddress snAP() { return _sys_prams.ap.sn; }
    String ssidAP() { return _sys_prams.ap.ssid; }
    String passAP() { return _sys_prams.ap.pass; }
    String dnsNameAP() { return _sys_prams.ap.dns_name; }
    uint8_t channelAP() { return _sys_prams.ap.channel; }
    uint8_t isHiddenAP() { return _sys_prams.ap.hidden; }
    uint8_t isDisableAP() { return _sys_prams.ap.disable; }
    void ipAPSet(IPAddress ip) { _sys_prams.ap.ip = ip; }
    void ipAPSet(const String &address) { _sys_prams.ap.ip.fromString(address); }
    void snAPSet(IPAddress sn) { _sys_prams.ap.sn = sn; }
    void snAPSet(const String &address) { _sys_prams.ap.sn.fromString(address); }
    void ssidAPSet(const String &ssid) { 
        ssid.toCharArray(_sys_prams.ap.ssid, SSID_LENGTH_MAX + 1);
    }
    void passAPSet(const String &pass) { 
        pass.toCharArray(_sys_prams.ap.pass, PASS_LENGTH_MAX + 1);
    }
    void dnsNameAPSet(const String &dnsName) { 
        dnsName.toCharArray(_sys_prams.ap.dns_name, HOSTNAME_LENGTH_MAX + 1);
    }
    void channelAPSet(uint8_t channel) { _sys_prams.ap.channel = channel; }
    void hiddenAPSet(uint8_t hidden) { _sys_prams.ap.hidden = hidden; }
    void disableAPSet(uint8_t disable) { _sys_prams.ap.disable = disable; }
    /* DDNS API */
    String serviceDDNS() { return _sys_prams.ddns.service; }
    String domainDDNS() { return _sys_prams.ddns.domain; }
    String userDDNS() { return _sys_prams.ddns.user; }
    String passDDNS() { return _sys_prams.ddns.pass; }
    String ipUrlDDNS() { return _sys_prams.ddns.ipUrl; }
    uint8_t syncTimeDDNS() { return _sys_prams.ddns.sync_time; }
    uint8_t disableDDNS() { return _sys_prams.ddns.disable; }
    void serviceDDNSSet(const String &service) { 
        service.toCharArray(_sys_prams.ddns.service, DDNS_SERVICE_LENGTH_MAX + 1);
    }
    void domainDDNSSet(const String &domain) { 
        domain.toCharArray(_sys_prams.ddns.domain, DDNS_DOMAIN_LENGTH_MAX + 1);
    }
    void userDDNSSet(const String &user) { 
        user.toCharArray(_sys_prams.ddns.user, DDNS_USER_LENGTH_MAX + 1);
    }
    void passDDNSSet(const String &pass) { 
        pass.toCharArray(_sys_prams.ddns.pass, DDNS_PASS_LENGTH_MAX + 1);
    }
    void ipUrlDDNSSet(const String &ipUrl) { 
        ipUrl.toCharArray(_sys_prams.ddns.ipUrl, DDNS_IPURL_LENGTH_MAX + 1);
    }
    void syncTimeDDNSSet(uint8_t sync_time) { _sys_prams.ddns.sync_time = sync_time; }
    void disableDDNSSet(uint8_t disable) { _sys_prams.ddns.disable = disable; }
    /* sntp API */
    String server1SNTP() { return _sys_prams.sntp.server1; }
    String server2SNTP() { return _sys_prams.sntp.server2; }
    String server3SNTP() { return _sys_prams.sntp.server3; }
    String TzTime() { return _sys_prams.sntp.TzTime; }
    long gmtOffsetSNTP() { return _sys_prams.sntp.gmtOffset; }
    int daylightOffsetSNTP() { return _sys_prams.sntp.daylightOffset; }
    int intervalSNTP() { return _sys_prams.sntp.interval; }
    void server1SNTPSet(const String &server) { 
        server.toCharArray(_sys_prams.sntp.server1, SNTP_SERVER_LENGTH_MAX + 1);
    }
    void server2SNTPSet(const String &server) { 
        server.toCharArray(_sys_prams.sntp.server2, SNTP_SERVER_LENGTH_MAX + 1);
    }
    void server3SNTPSet(const String &server) { 
        server.toCharArray(_sys_prams.sntp.server3, SNTP_SERVER_LENGTH_MAX + 1);
    }
    void gmtOffsetSNTPSet(long gmtOffset) { _sys_prams.sntp.gmtOffset = gmtOffset; }
    void daylightOffsetSNTPSet(int daylightOffset) { _sys_prams.sntp.daylightOffset = daylightOffset; }
    void intervalSNTPSet(int interval) { _sys_prams.sntp.interval = interval; }
    /* ping API */
    String pingHost(int id) { return _sys_prams.ping.host[id]; }
    int pingTimeout(int id) { return _sys_prams.ping.timeout[id]; }
    int pingInterval(int id) { return _sys_prams.ping.interval[id]; }
    int pingType(int id) { return _sys_prams.ping.type[id]; }
    bool pingEnable(int id) { return _sys_prams.ping.enable[id]; }
    void pingHostSet(const String &host, int id) { 
        host.toCharArray(_sys_prams.ping.host[id], PING_HOST_LENGTH_MAX + 1);
    }
    void pingTimeoutSet(int timeout, int id) { _sys_prams.ping.timeout[id] = timeout; }
    void pingIntervalSet(int interval, int id) { _sys_prams.ping.interval[id] = interval; }
    void pingTypeSet(int type, int id) { _sys_prams.ping.type[id] = type; }
    void pingEnableSet(bool enable, int id) { _sys_prams.ping.enable[id] = enable; }
};

extern ESPSysParams ESPConfig;

#endif // __ESP_SYS_PARAMS_H