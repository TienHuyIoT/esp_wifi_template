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
#define DDNS_SERVER_LENGTH_MAX  31
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
#define PASS_PU3_DEFAULT        1234
#define PASS_COST_DEFAULT       1234
#define PASS_LOGFILE_DEFAULT    1234
#define CONFIRM_NUM_MAX         5

/*Has to update the value of the element "version" of the espSysParamsDefault, file wifi_info_file.txt*/
#define SYS_PARAM_VERSION       1

typedef struct {
    struct{   
        uint32_t t_start;
        uint32_t t_stop;
        uint32_t t_start1;
        uint32_t t_stop1;
        uint32_t t_start2;
        uint32_t t_stop2;
    }log_time; 

    struct{   
        uint32_t t_start1;
        uint32_t t_stop1;
        uint32_t t_start2;
        uint32_t t_stop2;
        uint32_t t_start3;
        uint32_t t_stop3;
    }probe_time; 
    struct {
        uint32_t 	addr;
        uint16_t    sync_time;
    }i2c_para;
    struct {
        uint16_t 	udp;
        uint16_t 	tcp;
        uint16_t    ws;
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
    
    uint32_t confirm[CONFIRM_NUM_MAX];

    struct {
        IPAddress 	ip;
        IPAddress 	gw;
        IPAddress 	sn;
        IPAddress 	dns;
        char 		ssid[SSID_LENGTH_MAX + 1];
        char 		pass[PASS_LENGTH_MAX + 1];
        char        hostname[HOSTNAME_LENGTH_MAX+1];                
        uint8_t 	dhcp        : 1;
        uint8_t     disable     : 1;
        uint8_t     smart_cfg   : 1;
    }sta;
    struct {
        IPAddress   ip;
        IPAddress   sn; 
        char 		ssid[SSID_LENGTH_MAX + 1];
        char 		pass[PASS_LENGTH_MAX + 1];
        char        dns_name[HOSTNAME_LENGTH_MAX+1];               
        uint8_t 	channel : 4;
        uint8_t 	hidden  : 1;
        uint8_t     disable : 1;
    }ap;
    struct {
        char        service[DDNS_SERVICE_LENGTH_MAX + 1];
        char        server[DDNS_SERVER_LENGTH_MAX + 1];
        char        domain[DDNS_DOMAIN_LENGTH_MAX + 1];
        char        user[DDNS_USER_LENGTH_MAX + 1];
        char        pass[DDNS_PASS_LENGTH_MAX + 1];
        char        ipUrl[DDNS_IPURL_LENGTH_MAX + 1];
        uint8_t     sync_time;
        uint8_t     disable : 1;
    }ddns;
    struct {
        char        service[DDNS_SERVICE_LENGTH_MAX + 1];
        char        server[DDNS_SERVER_LENGTH_MAX + 1];
        char        domain[DDNS_DOMAIN_LENGTH_MAX + 1];
        char        user[DDNS_USER_LENGTH_MAX + 1];
        char        pass[DDNS_PASS_LENGTH_MAX + 1];
        char        ipUrl[DDNS_IPURL_LENGTH_MAX + 1];
        uint8_t     sync_time;
        uint8_t     disable : 1;
    }ddns_private;
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
    struct {
        int hw_version;
        uint32_t CpuFrequencyMhz;
        struct {
            int8_t Addr;
            int8_t RstIO;
            bool Enable;
        } LAN;
        struct {
            int8_t PIN;
            int8_t OffState;
            bool Enable;
        } LED;
        struct {
            uint32_t Freq;
            int8_t Sda;
            int8_t Scl;
            uint8_t TimeScan;
            bool Enable;
        } I2C;
        struct {
            int Baud;
            int8_t Serial;
            int8_t Tx;
            int8_t Rx;
            int8_t Rts;
            bool Enable;
        } PROBE;
        struct {
            int Baud;
            int8_t Serial;
            int8_t Tx;
            int8_t Rx;
            int8_t Rts;
            bool Enable;
        } RFID;
        struct {
            int Baud;
            int8_t Serial;
            int8_t Tx;
            int8_t Rx;
            int8_t Rts;
            bool Enable;
        } PRINTER;
    } hw_cfg;
    int fw_version;
    uint8_t version;
} esp_sys_params_t;

class ESPSysParams
{
    using File = fs::File;
    using FS = fs::FS;
public:
    typedef enum : uint8_t {
        CONFIRM_COMMON = 0,
        CONFIRM_PU3,
        CONFIRM_COST,
        CONFIRM_LOGFILE,
        CONFIRM_RESERVE
    } passConfirm_t;

    typedef enum : uint8_t {
        OK_STATUS = 0,
        ERROR_STATUS,
        UPDATE_STATUS,
        ADD_VERION_STATUS,
        NEW_VERSION_STATUS,
        ADD_FIRMWARE_STATUS,
        NEW_FIRMWARE_STATUS,
        ADD_HARDWARE_STATUS,
        NEW_HARDWARE_STATUS
    } syncStatus_t;

private:
    FS *_fs;
    esp_sys_params_t _sys_prams;
    uint32_t _statusUpdate;
    uint8_t syncFromFileSystem();
    void saveToFileSystem();
public:
    ESPSysParams(FS &fs);
    ~ESPSysParams();

    void load(FS* fs = nullptr);
    void save() { saveToFileSystem(); }
    void setDefault() { _fs->remove(ESP_SYSTEM_PARAMS); }
    void resetPassword();
    boolean passSupperAdminIsOK(const String &pass);

    boolean isUpdated(uint8_t status) { return bitRead(_statusUpdate, status); }
    /* log_time API */
    uint32_t logTimeStart() const { return _sys_prams.log_time.t_start; }
    uint32_t logTimeStart1() const { return _sys_prams.log_time.t_start1; }
    uint32_t logTimeStart2() const { return _sys_prams.log_time.t_start2; }
    uint32_t logTimeStop() const { return _sys_prams.log_time.t_stop; }
    uint32_t logTimeStop1() const { return _sys_prams.log_time.t_stop1; }
    uint32_t logTimeStop2() const { return _sys_prams.log_time.t_stop2; }
    void logTimeStartSet(uint32_t tStart) { _sys_prams.log_time.t_start = tStart; }
    void logTimeStart1Set(uint32_t tStart) { _sys_prams.log_time.t_start1 = tStart; }
    void logTimeStart2Set(uint32_t tStart) { _sys_prams.log_time.t_start2 = tStart; }
    void logTimeStopSet(uint32_t tStop) { _sys_prams.log_time.t_stop = tStop; }
    void logTimeStop1Set(uint32_t tStop) { _sys_prams.log_time.t_stop1 = tStop; }
    void logTimeStop2Set(uint32_t tStop) { _sys_prams.log_time.t_stop2 = tStop; }
    /* probe_time API */
    uint32_t probeTimeStart1() const { return _sys_prams.probe_time.t_start1; }
    uint32_t probeTimeStart2() const { return _sys_prams.probe_time.t_start2; }
    uint32_t probeTimeStart3() const { return _sys_prams.probe_time.t_start3; }
    uint32_t probeTimeStop1() const { return _sys_prams.probe_time.t_stop1; }
    uint32_t probeTimeStop2() const { return _sys_prams.probe_time.t_stop2; }
    uint32_t probeTimeStop3() const { return _sys_prams.probe_time.t_stop3; }
    void probeTimeStart1Set(uint32_t tStart) { _sys_prams.probe_time.t_start1 = tStart; }
    void probeTimeStart2Set(uint32_t tStart) { _sys_prams.probe_time.t_start2 = tStart; }
    void probeTimeStart3Set(uint32_t tStart) { _sys_prams.probe_time.t_start3 = tStart; }
    void probeTimeStop1Set(uint32_t tStop) { _sys_prams.probe_time.t_stop1 = tStop; }
    void probeTimeStop2Set(uint32_t tStop) { _sys_prams.probe_time.t_stop2 = tStop; }
    void probeTimeStop3Set(uint32_t tStop) { _sys_prams.probe_time.t_stop3 = tStop; }
    /* i2c_para API */
    uint8_t i2cDeviceNum();
    uint32_t i2cDeviceAddr() const { return _sys_prams.i2c_para.addr; }
    bool i2cDeviceIsEnable(uint8_t addr) const { return bitRead(_sys_prams.i2c_para.addr, addr); }
    uint16_t i2cParaInterval() const { return _sys_prams.i2c_para.sync_time; }
    void i2cParaAddrSet(uint32_t addr) { _sys_prams.i2c_para.addr = addr; }
    void i2cParaIntervalSet(uint16_t interval) { _sys_prams.i2c_para.sync_time = interval; }
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
    uint32_t passConfirm(passConfirm_t passType) { 
        return _sys_prams.confirm[passType]; 
    }
    void passConfirmSet(passConfirm_t passType, uint32_t pass) { 
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
    String serverDDNS() { return _sys_prams.ddns.server; }
    String domainDDNS() { return _sys_prams.ddns.domain; }
    String userDDNS() { return _sys_prams.ddns.user; }
    String passDDNS() { return _sys_prams.ddns.pass; }
    String ipUrlDDNS() { return _sys_prams.ddns.ipUrl; }
    uint8_t syncTimeDDNS() { return _sys_prams.ddns.sync_time; }
    uint8_t disableDDNS() { return _sys_prams.ddns.disable; }
    void serviceDDNSSet(const String &service) { 
        service.toCharArray(_sys_prams.ddns.service, DDNS_SERVICE_LENGTH_MAX + 1);
    }
    void serverDDNSSet(const String &server) { 
        server.toCharArray(_sys_prams.ddns.server, DDNS_SERVER_LENGTH_MAX + 1);
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

    /* DDNS private API */
    String serviceDDNSPri() { return _sys_prams.ddns_private.service; }
    String serverDDNSPri() { return _sys_prams.ddns_private.server; }
    String domainDDNSPri() { return _sys_prams.ddns_private.domain; }
    String userDDNSPri() { return _sys_prams.ddns_private.user; }
    String passDDNSPri() { return _sys_prams.ddns_private.pass; }
    String ipUrlDDNSPri() { return _sys_prams.ddns_private.ipUrl; }
    uint8_t syncTimeDDNSPri() { return _sys_prams.ddns_private.sync_time; }
    uint8_t disableDDNSPri() { return _sys_prams.ddns_private.disable; }
    void serviceDDNSPriSet(const String &service) { 
        service.toCharArray(_sys_prams.ddns_private.service, DDNS_SERVICE_LENGTH_MAX + 1);
    }
    void serverDDNSPriSet(const String &server) { 
        server.toCharArray(_sys_prams.ddns_private.server, DDNS_SERVER_LENGTH_MAX + 1);
    }
    void domainDDNSPriSet(const String &domain) { 
        domain.toCharArray(_sys_prams.ddns_private.domain, DDNS_DOMAIN_LENGTH_MAX + 1);
    }
    void userDDNSPriSet(const String &user) { 
        user.toCharArray(_sys_prams.ddns_private.user, DDNS_USER_LENGTH_MAX + 1);
    }
    void passDDNSPriSet(const String &pass) { 
        pass.toCharArray(_sys_prams.ddns_private.pass, DDNS_PASS_LENGTH_MAX + 1);
    }
    void ipUrlDDNSPriSet(const String &ipUrl) { 
        ipUrl.toCharArray(_sys_prams.ddns_private.ipUrl, DDNS_IPURL_LENGTH_MAX + 1);
    }
    void syncTimeDDNSPriSet(uint8_t sync_time) { _sys_prams.ddns_private.sync_time = sync_time; }
    void disableDDNSPriSet(uint8_t disable) { _sys_prams.ddns_private.disable = disable; }

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

    int fwVersionGet() { return _sys_prams.fw_version; }
    void fwVersionSet(int fw_version) { _sys_prams.fw_version = fw_version; }

    uint32_t CpuFrequencyMhzGet() { return _sys_prams.hw_cfg.CpuFrequencyMhz; }
    void CpuFrequencyMhzSet(uint32_t freq) { _sys_prams.hw_cfg.CpuFrequencyMhz = freq; }
    
    int hwVersionGet() { return _sys_prams.hw_cfg.hw_version; }
    String hwVersionStr() { return "V" + String(_sys_prams.hw_cfg.hw_version/10) + "."  + String(_sys_prams.hw_cfg.hw_version%10); }
    void hwVersionSet(int hw_version) { _sys_prams.hw_cfg.hw_version = hw_version; }
    String hardwareId();

    bool ledEnableGet() { return _sys_prams.hw_cfg.LED.Enable; }
    int8_t ledOffStateGet() { return _sys_prams.hw_cfg.LED.OffState; }
    int8_t ledPINGet() { return _sys_prams.hw_cfg.LED.PIN; }

    bool i2cEnableGet() { return _sys_prams.hw_cfg.I2C.Enable; }
    uint32_t i2cFreqGet() { return _sys_prams.hw_cfg.I2C.Freq; }
    int8_t i2CSdaGet() { return _sys_prams.hw_cfg.I2C.Sda; }
    int8_t i2cSclGet() { return _sys_prams.hw_cfg.I2C.Scl; }
    int8_t i2cTimeScanGet() { return _sys_prams.hw_cfg.I2C.TimeScan; }

    bool printerEnableGet() { return _sys_prams.hw_cfg.PRINTER.Enable; }
    int printerBaudGet() { return _sys_prams.hw_cfg.PRINTER.Baud; }
    int8_t printerSerialGet() { return _sys_prams.hw_cfg.PRINTER.Serial; }
    int8_t printerTxGet() { return _sys_prams.hw_cfg.PRINTER.Tx; }
    int8_t printerRxGet() { return _sys_prams.hw_cfg.PRINTER.Rx; }
    int8_t printerRrsGet() { return _sys_prams.hw_cfg.PRINTER.Rts; }
    void printerEnableSet(bool var) { _sys_prams.hw_cfg.PRINTER.Enable = var; }
    void printerBaudSet(int var) { _sys_prams.hw_cfg.PRINTER.Baud = var; }
    void printerSerialSet(int8_t var) { _sys_prams.hw_cfg.PRINTER.Serial = var; }
    void printerTxSet(int8_t var) { _sys_prams.hw_cfg.PRINTER.Tx = var; }
    void printerRxSet(int8_t var) { _sys_prams.hw_cfg.PRINTER.Rx = var; }
    void printerRrsSet(int8_t var) { _sys_prams.hw_cfg.PRINTER.Rts = var; }

    bool rfidEnableGet() { return _sys_prams.hw_cfg.RFID.Enable; }
    int rfidBaudGet() { return _sys_prams.hw_cfg.RFID.Baud; }
    int8_t rfidSerialGet() { return _sys_prams.hw_cfg.RFID.Serial; }
    int8_t rfidTxGet() { return _sys_prams.hw_cfg.RFID.Tx; }
    int8_t rfidRxGet() { return _sys_prams.hw_cfg.RFID.Rx; }
    int8_t rfidRrsGet() { return _sys_prams.hw_cfg.RFID.Rts; }
    void rfidEnableSet(bool var) { _sys_prams.hw_cfg.RFID.Enable = var; }
    void rfidBaudSet(int var) { _sys_prams.hw_cfg.RFID.Baud = var; }
    void rfidSerialSet(int8_t var) { _sys_prams.hw_cfg.RFID.Serial = var; }
    void rfidTxSet(int8_t var) { _sys_prams.hw_cfg.RFID.Tx = var; }
    void rfidRxSet(int8_t var) { _sys_prams.hw_cfg.RFID.Rx = var; }
    void rfidRrsSet(int8_t var) { _sys_prams.hw_cfg.RFID.Rts = var; }

    bool probeEnableGet() { return _sys_prams.hw_cfg.PROBE.Enable; }
    int probeBaudGet() { return _sys_prams.hw_cfg.PROBE.Baud; }
    int8_t probeSerialGet() { return _sys_prams.hw_cfg.PROBE.Serial; }
    int8_t probeTxGet() { return _sys_prams.hw_cfg.PROBE.Tx; }
    int8_t probeRxGet() { return _sys_prams.hw_cfg.PROBE.Rx; }
    int8_t probeRrsGet() { return _sys_prams.hw_cfg.PROBE.Rts; }
    void probeEnableSet(bool var) { _sys_prams.hw_cfg.PROBE.Enable = var; }
    void probeBaudSet(int var) { _sys_prams.hw_cfg.PROBE.Baud = var; }
    void probeSerialSet(int8_t var) { _sys_prams.hw_cfg.PROBE.Serial = var; }
    void probeTxSet(int8_t var) { _sys_prams.hw_cfg.PROBE.Tx = var; }
    void probeRxSet(int8_t var) { _sys_prams.hw_cfg.PROBE.Rx = var; }
    void probeRrsSet(int8_t var) { _sys_prams.hw_cfg.PROBE.Rts = var; }

    uint8_t versionGet() { return _sys_prams.version; }
    void versionGet(uint8_t version) { _sys_prams.version = version; }
};

extern ESPSysParams ESPConfig;

#endif // __ESP_SYS_PARAMS_H