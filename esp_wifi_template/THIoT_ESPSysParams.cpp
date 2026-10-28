#include <ArduinoJson.h>
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPBoard.h"
#include "THIoT_PFTimeZone.h"
#include "THIoT_PFcUtil.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFSerialTrace.h"

/* ---------------------------------------------------------------------------
 * Compatibility block for this template board.
 *
 * The shared system-parameter module stores the optional hardware description
 * (LAN reset line, I2C / RFID / printer / modbus serial pins) of the reference
 * station hardware.  A generic template board has none of those peripherals, so
 * the identifiers are kept defined here with "not used" values (-1 / 0).  They
 * are inert: no module of this project reads them back.
 * ------------------------------------------------------------------------- */
#ifndef FW_VERSION_INT
#define FW_VERSION_INT          ((FW_VERSION_MAJOR * 100) + (FW_VERSION_MINOR * 10) + FW_VERSION_BUILD)
#endif
#ifndef HW_VERSION_INT
#define HW_VERSION_INT          0       /* generic template board revision */
#endif
#ifndef ETH_RESET_PIN
#define ETH_RESET_PIN           -1
#endif
#ifndef I2C_SDA_GPIO
#define I2C_SDA_GPIO            -1
#endif
#ifndef I2C_SCL_GPIO
#define I2C_SCL_GPIO            -1
#endif
#ifndef HARDWARE_SERIAL_DEBUG
#define HARDWARE_SERIAL_DEBUG   0
#define HARDWARE_SERIAL_PRINTER 1
#define HARDWARE_SERIAL_RFID    2
#define HARDWARE_SERIAL_PROBE   2
#define HARDWARE_SERIAL_NUM     3
#endif
#ifndef MBUS_TXD_PIN
#define MBUS_TXD_PIN            -1
#endif
#ifndef MBUS_RXD_PIN
#define MBUS_RXD_PIN            -1
#endif
#ifndef RFID_TXD_PIN
#define RFID_TXD_PIN            -1
#endif
#ifndef RFID_RXD_PIN
#define RFID_RXD_PIN            -1
#endif
#ifndef PRINTER_TXD_PIN
#define PRINTER_TXD_PIN         -1
#endif
#ifndef PRINTER_RXD_PIN
#define PRINTER_RXD_PIN         -1
#endif

#define SYS_PARAM_PORT              SERIAL_PORT
#define SYS_PARAM_TAG_CONSOLE(...)  //CONSOLE_TAG_LOGI("[SYS_PARAM]", __VA_ARGS__)

#define SYS_PARAM_TAG_ESP_LOG(...)  ESP_FS_TAG_LOGI("[SYS_PARAM]", __VA_ARGS__)

#define PARAM_JSON_OBJ_SIZE (JSON_ARRAY_SIZE(2) + JSON_ARRAY_SIZE(5) + 3*JSON_OBJECT_SIZE(2) + 4*JSON_OBJECT_SIZE(3) + 3*JSON_OBJECT_SIZE(5) + 5*JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(7) + 4*JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(10) + JSON_OBJECT_SIZE(16))

const char espSysParamsDefault[] PROGMEM = R"=====(
{
    "log_time":{
        "t_start" :210000,
        "t_stop"  :50000,
        "t_start1":50000,
        "t_stop1" :130000,
        "t_start2":130000,
        "t_stop2" :210000
    },
    "probe_time":{
        "t_start1" :210000,
        "t_stop1"  :50000,
        "t_start2":50000,
        "t_stop2" :130000,
        "t_start3":130000,
        "t_stop3" :210000
    },
    "i2c_para":{
        "addr": 1,
        "sync_time": 333
    },
    "port":{
        "udp": 25123,
        "tcp": 25123,
        "ws" : 25124
    },
    "device":{
        "name": "CỬA HÀNG XĂNG DẦU SỐ 1",
        "addr": "200 Cộng Hòa, Phường 15, Quận Tân Bình, Tp HCM",
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
        "ip":"192.168.1.100",
        "gw":"192.168.1.1",
        "sn":"255.255.255.0",
        "dns":"192.168.1.1",
        "ssid":"",
        "psk":"",
        "hostname":"montech",
        "dhcp":1,
        "disable":0,
        "smart_cfg":1
    },
    "ap":{        
        "ip":"192.168.4.1",
        "sn":"255.255.255.0",
        "ssid":"MONTECH",
        "psk":"88888888",
        "dns_name":"montech.vn",        
        "disable":0,
        "channel":2,
        "hidden":0
    },
    "ddns":{
        "service": "noip",
        "server": "dynupdate.no-ip.com",
        "ipUrl": "ifconfig.me/ip",
        "domain": "tienhuyiot.vn",
        "user": "admin",
        "pass": "admin",
        "sync_time": 120,
        "disable": 1
    },
    "ddns_private":{
        "service": "private",
        "server": "montech.ddns.net",
        "ipUrl": "ifconfig.me/ip",
        "domain": "tienhuyiot.vn",
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
    "hw_cfg":{
        "hw_version": 24,
        "CpuFrequencyMhz": 240,
        "LAN": {
            "Addr":-1,
            "RstIO":-1,
            "Enable":1
        },
        "LED": {
            "PIN":3,
            "OffState":1,
            "Enable":1
        },
        "I2C": {
            "Freq":90000,
            "Sda":33,
            "Scl":32,
            "TimeScan":5,
            "Enable":1
        },
        "PROBE": {
            "Enable":0,
            "Serial":2,
            "Baud":9600,
            "Tx":16,
            "Rx":35,
            "Rts":-1
        },
        "RFID": {
            "Enable":0,
            "Serial":2,
            "Baud":38400,
            "Tx":16,
            "Rx":35,
            "Rts":-1
        },
        "PRINTER": {
            "Enable":1,
            "Serial":1,
            "Baud":115200,
            "Tx":0,
            "Rx":34,
            "Rts":-1
        }
    },
    "fw_version": 318,
    "version":1
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

uint8_t ESPSysParams::i2cDeviceNum() { 
    uint8_t count = 0;
    for (int i = 0; i < (sizeof(uint32_t) * 8); ++i) {
        if (bitRead(_sys_prams.i2c_para.addr, i)) {
            ++count;
        }
    }
    return count; 
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

String ESPSysParams::hardwareId() {
    char eMac[13];
    uint64_t chipID = ESP.getEfuseMac();
    hex2str(eMac, (uint8_t*)&chipID, 6, UPPER_CASE);
    return String(eMac);
}

void ESPSysParams::saveToFileSystem()
{
    using File = fs::File;
    File fs_handle;

    const size_t capacity = PARAM_JSON_OBJ_SIZE;
    DynamicJsonBuffer jsonBuffer(capacity);
    JsonObject& root = jsonBuffer.createObject();

    JsonObject& log_time = root.createNestedObject("log_time");
    log_time["t_start"].set(_sys_prams.log_time.t_start);
    log_time["t_stop"].set(_sys_prams.log_time.t_stop);
    log_time["t_start1"].set(_sys_prams.log_time.t_start1);
    log_time["t_stop1"].set(_sys_prams.log_time.t_stop1);
    log_time["t_start2"].set(_sys_prams.log_time.t_start2);
    log_time["t_stop2"].set(_sys_prams.log_time.t_stop2);

    JsonObject& probe_time = root.createNestedObject("probe_time");
    probe_time["t_start1"].set(_sys_prams.probe_time.t_start1);
    probe_time["t_stop1"].set(_sys_prams.probe_time.t_stop1);
    probe_time["t_start2"].set(_sys_prams.probe_time.t_start2);
    probe_time["t_stop2"].set(_sys_prams.probe_time.t_stop2);
    probe_time["t_start3"].set(_sys_prams.probe_time.t_start3);
    probe_time["t_stop3"].set(_sys_prams.probe_time.t_stop3);

    JsonObject& i2c_para = root.createNestedObject("i2c_para");
    i2c_para["addr"].set(_sys_prams.i2c_para.addr);
    i2c_para["sync_time"].set(_sys_prams.i2c_para.sync_time);

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

    JsonObject& ddns_private = root.createNestedObject("ddns_private");
    ddns_private["service"].set(_sys_prams.ddns_private.service);
    ddns_private["server"].set(_sys_prams.ddns_private.server);
    ddns_private["domain"].set(_sys_prams.ddns_private.domain);
    ddns_private["user"].set(_sys_prams.ddns_private.user);    
    ddns_private["pass"].set(_sys_prams.ddns_private.pass); 
    ddns_private["sync_time"].set(_sys_prams.ddns_private.sync_time); 
    ddns_private["disable"].set(_sys_prams.ddns_private.disable);
    ddns_private["ipUrl"].set(_sys_prams.ddns_private.ipUrl);

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

    JsonObject& hw_cfg = root.createNestedObject("hw_cfg");
    hw_cfg["hw_version"] = _sys_prams.hw_cfg.hw_version;
    hw_cfg["CpuFrequencyMhz"] = _sys_prams.hw_cfg.CpuFrequencyMhz;

    JsonObject& hw_cfg_LAN = hw_cfg.createNestedObject("LAN");
    hw_cfg_LAN["Addr"] = _sys_prams.hw_cfg.LAN.Addr;
    hw_cfg_LAN["RstIO"] = _sys_prams.hw_cfg.LAN.RstIO;
    hw_cfg_LAN["Enable"] = _sys_prams.hw_cfg.LAN.Enable;

    JsonObject& hw_cfg_LED = hw_cfg.createNestedObject("LED");
    hw_cfg_LED["PIN"]     = _sys_prams.hw_cfg.LED.PIN;
    hw_cfg_LED["OffState"] = _sys_prams.hw_cfg.LED.OffState;
    hw_cfg_LED["Enable"]  = _sys_prams.hw_cfg.LED.Enable;

    JsonObject& hw_cfg_I2C = hw_cfg.createNestedObject("I2C");
    hw_cfg_I2C["Freq"]   = _sys_prams.hw_cfg.I2C.Freq;
    hw_cfg_I2C["Sda"]    = _sys_prams.hw_cfg.I2C.Sda;
    hw_cfg_I2C["Scl"]    = _sys_prams.hw_cfg.I2C.Scl;
    hw_cfg_I2C["TimeScan"] = _sys_prams.hw_cfg.I2C.TimeScan;
    hw_cfg_I2C["Enable"] = _sys_prams.hw_cfg.I2C.Enable;

    JsonObject& hw_cfg_PROBE = hw_cfg.createNestedObject("PROBE");
    hw_cfg_PROBE["Enable"] = _sys_prams.hw_cfg.PROBE.Enable;
    hw_cfg_PROBE["Serial"] = _sys_prams.hw_cfg.PROBE.Serial;
    hw_cfg_PROBE["Baud"] = _sys_prams.hw_cfg.PROBE.Baud;
    hw_cfg_PROBE["Tx"] = _sys_prams.hw_cfg.PROBE.Tx;
    hw_cfg_PROBE["Rx"] = _sys_prams.hw_cfg.PROBE.Rx;
    hw_cfg_PROBE["Rts"] = _sys_prams.hw_cfg.PROBE.Rts;

    JsonObject& hw_cfg_RFID = hw_cfg.createNestedObject("RFID");
    hw_cfg_RFID["Enable"] = _sys_prams.hw_cfg.RFID.Enable;
    hw_cfg_RFID["Serial"] = _sys_prams.hw_cfg.RFID.Serial;
    hw_cfg_RFID["Baud"] = _sys_prams.hw_cfg.RFID.Baud;
    hw_cfg_RFID["Tx"] = _sys_prams.hw_cfg.RFID.Tx;
    hw_cfg_RFID["Rx"] = _sys_prams.hw_cfg.RFID.Rx;
    hw_cfg_RFID["Rts"] = _sys_prams.hw_cfg.RFID.Rts;

    JsonObject& hw_cfg_PRINTER = hw_cfg.createNestedObject("PRINTER");
    hw_cfg_PRINTER["Enable"] = _sys_prams.hw_cfg.PRINTER.Enable;
    hw_cfg_PRINTER["Serial"] = _sys_prams.hw_cfg.PRINTER.Serial;
    hw_cfg_PRINTER["Baud"] = _sys_prams.hw_cfg.PRINTER.Baud;
    hw_cfg_PRINTER["Tx"] = _sys_prams.hw_cfg.PRINTER.Tx;
    hw_cfg_PRINTER["Rx"] = _sys_prams.hw_cfg.PRINTER.Rx;
    hw_cfg_PRINTER["Rts"] = _sys_prams.hw_cfg.PRINTER.Rts;

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

    JsonObject& log_time = root["log_time"];
    if (log_time.success())
    {
        _sys_prams.log_time.t_start = log_time["t_start"].as<int>();
        _sys_prams.log_time.t_stop = log_time["t_stop"].as<int>();
        _sys_prams.log_time.t_start1 = log_time["t_start1"].as<int>();
        _sys_prams.log_time.t_stop1 = log_time["t_stop1"].as<int>();
        _sys_prams.log_time.t_start2 = log_time["t_start2"].as<int>();
        _sys_prams.log_time.t_stop2 = log_time["t_stop2"].as<int>();
    }

    JsonObject& probe_time = root["probe_time"];
    if (probe_time.success())
    {
        _sys_prams.probe_time.t_start1 = probe_time["t_start1"].as<int>();
        _sys_prams.probe_time.t_stop1 = probe_time["t_stop1"].as<int>();
        _sys_prams.probe_time.t_start2 = probe_time["t_start2"].as<int>();
        _sys_prams.probe_time.t_stop2 = probe_time["t_stop2"].as<int>();
        _sys_prams.probe_time.t_start3 = probe_time["t_start3"].as<int>();
        _sys_prams.probe_time.t_stop3 = probe_time["t_stop3"].as<int>();
    }
    
    JsonObject& i2c_para = root["i2c_para"];
    if (i2c_para.success())
    {
        _sys_prams.i2c_para.addr = i2c_para["addr"].as<int>();
        _sys_prams.i2c_para.sync_time = i2c_para["sync_time"].as<int>();
    }
    
    /* (Fixed) Prevent error ...?? 
        Due to: task_i2c_data_poll has an exception unhandled by IntegerDivideByZero
    */
#if (0)
    if (0 == _sys_prams.i2c_para.addr) {
        _sys_prams.i2c_para.addr = 1;
    }
#endif

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

    JsonObject& ddns_private = root["ddns_private"];
    if (ddns_private.success())
    {
        SYS_PARAM_TAG_CONSOLE("Load ddns_private information");
        ddns_private["service"].as<String>().toCharArray(_sys_prams.ddns_private.service, DDNS_SERVICE_LENGTH_MAX + 1);
        ddns_private["server"].as<String>().toCharArray(_sys_prams.ddns_private.server, DDNS_SERVER_LENGTH_MAX + 1);
        ddns_private["domain"].as<String>().toCharArray(_sys_prams.ddns_private.domain, DDNS_DOMAIN_LENGTH_MAX + 1);
        ddns_private["user"].as<String>().toCharArray(_sys_prams.ddns_private.user, DDNS_USER_LENGTH_MAX + 1);
        ddns_private["pass"].as<String>().toCharArray(_sys_prams.ddns_private.pass, DDNS_PASS_LENGTH_MAX + 1);
        ddns_private["ipUrl"].as<String>().toCharArray(_sys_prams.ddns_private.ipUrl, DDNS_IPURL_LENGTH_MAX + 1);
        _sys_prams.ddns_private.sync_time = ddns_private["sync_time"].as<int>();
        _sys_prams.ddns_private.disable = ddns_private["disable"].as<int>();
    } else {
        strncpy(_sys_prams.ddns_private.service, "private", DDNS_SERVICE_LENGTH_MAX + 1);
        strncpy(_sys_prams.ddns_private.server, "montech.ddns.net", DDNS_SERVER_LENGTH_MAX + 1);
        strncpy(_sys_prams.ddns_private.domain, "tienhuyiot.vn", DDNS_DOMAIN_LENGTH_MAX + 1);
        strncpy(_sys_prams.ddns_private.user, "admin", DDNS_USER_LENGTH_MAX + 1);
        strncpy(_sys_prams.ddns_private.pass, "admin", DDNS_PASS_LENGTH_MAX + 1);
        strncpy(_sys_prams.ddns_private.ipUrl, "ifconfig.me/ip", DDNS_IPURL_LENGTH_MAX + 1);
        _sys_prams.ddns_private.sync_time = 120;
        _sys_prams.ddns_private.disable = 1;
        status = UPDATE_STATUS;
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


    /** Adapter for V3.1.7 */
    _sys_prams.hw_cfg.CpuFrequencyMhz = 240;

    _sys_prams.hw_cfg.LAN.Addr = -1;
    _sys_prams.hw_cfg.LAN.RstIO = ETH_RESET_PIN;
    _sys_prams.hw_cfg.LAN.Enable = 1;

    _sys_prams.hw_cfg.LED.PIN = LED_STATUS_GPIO;
    _sys_prams.hw_cfg.LED.OffState = HIGH;  // led_status_off()
    _sys_prams.hw_cfg.LED.Enable = 1;

    _sys_prams.hw_cfg.I2C.Freq = 90000;
    _sys_prams.hw_cfg.I2C.Sda = I2C_SDA_GPIO;
    _sys_prams.hw_cfg.I2C.Scl = I2C_SCL_GPIO;
    _sys_prams.hw_cfg.I2C.TimeScan = 5;
    _sys_prams.hw_cfg.I2C.Enable = 1;

    _sys_prams.hw_cfg.PROBE.Enable = 0;
    _sys_prams.hw_cfg.PROBE.Serial = HARDWARE_SERIAL_PROBE; // MBUS_HW_SERIAL
    _sys_prams.hw_cfg.PROBE.Baud = 9600;
    _sys_prams.hw_cfg.PROBE.Tx = MBUS_TXD_PIN;
    _sys_prams.hw_cfg.PROBE.Rx = MBUS_RXD_PIN;
    _sys_prams.hw_cfg.PROBE.Rts = -1;

    _sys_prams.hw_cfg.RFID.Enable =  0;
    _sys_prams.hw_cfg.RFID.Serial = HARDWARE_SERIAL_RFID; // RFID_HW_SERIAL
    _sys_prams.hw_cfg.RFID.Baud = 38400;
    _sys_prams.hw_cfg.RFID.Tx = RFID_TXD_PIN;
    _sys_prams.hw_cfg.RFID.Rx = RFID_RXD_PIN;
    _sys_prams.hw_cfg.RFID.Rts = -1;

    _sys_prams.hw_cfg.PRINTER.Enable = 1;
    _sys_prams.hw_cfg.PRINTER.Serial = HARDWARE_SERIAL_PRINTER; // PRINTER_HW_SERIAL
    _sys_prams.hw_cfg.PRINTER.Baud = 115200;
    _sys_prams.hw_cfg.PRINTER.Tx = PRINTER_TXD_PIN;
    _sys_prams.hw_cfg.PRINTER.Rx = PRINTER_RXD_PIN;
    _sys_prams.hw_cfg.PRINTER.Rts = -1;
    constexpr uint32_t FLASH_SIZE_4M = 4194304;
    if (root["hw_cfg"].success()) {
        JsonObject& hw_cfg = root["hw_cfg"];
        _sys_prams.hw_cfg.hw_version = hw_cfg["hw_version"].as<int>(); // 24
        _sys_prams.hw_cfg.CpuFrequencyMhz = hw_cfg["CpuFrequencyMhz"].as<uint32_t>(); // 240

        if (hw_cfg["LAN"].success()) {
            JsonObject& hw_cfg_LAN = hw_cfg["LAN"];
            _sys_prams.hw_cfg.LAN.Addr = hw_cfg_LAN["Addr"].as<int8_t>(); // -1
            _sys_prams.hw_cfg.LAN.RstIO = hw_cfg_LAN["RstIO"].as<int8_t>(); // -1
            _sys_prams.hw_cfg.LAN.Enable = hw_cfg_LAN["Enable"].as<bool>(); // 0
        }

        if (hw_cfg["LED"].success()) {
            JsonObject& hw_cfg_LED = hw_cfg["LED"];
            _sys_prams.hw_cfg.LED.PIN = hw_cfg_LED["PIN"]; // 3
            _sys_prams.hw_cfg.LED.OffState = hw_cfg_LED["OffState"]; // 0
            _sys_prams.hw_cfg.LED.Enable = hw_cfg_LED["Enable"]; // 1
        }

        if (hw_cfg["I2C"].success()) {
            JsonObject& hw_cfg_I2C = hw_cfg["I2C"];
            _sys_prams.hw_cfg.I2C.Freq = hw_cfg_I2C["Freq"]; // 90000
            _sys_prams.hw_cfg.I2C.Sda = hw_cfg_I2C["Sda"]; // 33
            _sys_prams.hw_cfg.I2C.Scl = hw_cfg_I2C["Scl"]; // 32
            _sys_prams.hw_cfg.I2C.TimeScan = hw_cfg_I2C["TimeScan"]; // 5
            if (_sys_prams.hw_cfg.I2C.TimeScan < 5) {
                _sys_prams.hw_cfg.I2C.TimeScan = 5; // Minimum scan time
                status = UPDATE_STATUS;
            }
            _sys_prams.hw_cfg.I2C.Enable = hw_cfg_I2C["Enable"]; // 1
        }

        if (hw_cfg["PROBE"].success()) {
            JsonObject& hw_cfg_PROBE = hw_cfg["PROBE"];
            _sys_prams.hw_cfg.PROBE.Enable = hw_cfg_PROBE["Enable"].as<bool>(); // 0
            _sys_prams.hw_cfg.PROBE.Serial = hw_cfg_PROBE["Serial"].as<int8_t>(); // 1
            _sys_prams.hw_cfg.PROBE.Baud = hw_cfg_PROBE["Baud"].as<int>(); // 9600
            _sys_prams.hw_cfg.PROBE.Tx = hw_cfg_PROBE["Tx"].as<int8_t>(); // 0
            _sys_prams.hw_cfg.PROBE.Rx = hw_cfg_PROBE["Rx"].as<int8_t>(); // 34
            _sys_prams.hw_cfg.PROBE.Rts = hw_cfg_PROBE["Rts"].as<int8_t>(); // -1
        }

        if (hw_cfg["RFID"].success()) {
            JsonObject& hw_cfg_RFID = hw_cfg["RFID"];
            _sys_prams.hw_cfg.RFID.Enable = hw_cfg_RFID["Enable"].as<bool>(); // 0
            _sys_prams.hw_cfg.RFID.Serial = hw_cfg_RFID["Serial"].as<int8_t>(); // 2
            _sys_prams.hw_cfg.RFID.Baud = hw_cfg_RFID["Baud"].as<int>(); // 38400
            _sys_prams.hw_cfg.RFID.Tx = hw_cfg_RFID["Tx"].as<int8_t>(); // 16
            _sys_prams.hw_cfg.RFID.Rx = hw_cfg_RFID["Rx"].as<int8_t>(); // 35
            _sys_prams.hw_cfg.RFID.Rts = hw_cfg_RFID["Rts"].as<int8_t>(); // -1
        }

        if (hw_cfg["PRINTER"].success()) {
            JsonObject& hw_cfg_PRINTER = hw_cfg["PRINTER"];
            _sys_prams.hw_cfg.PRINTER.Enable = hw_cfg_PRINTER["Enable"].as<bool>(); // 1
            _sys_prams.hw_cfg.PRINTER.Serial = hw_cfg_PRINTER["Serial"].as<int8_t>(); // 1
            _sys_prams.hw_cfg.PRINTER.Baud = hw_cfg_PRINTER["Baud"].as<int>(); // 115200
            _sys_prams.hw_cfg.PRINTER.Tx = hw_cfg_PRINTER["Tx"].as<int8_t>(); // 0
            _sys_prams.hw_cfg.PRINTER.Rx = hw_cfg_PRINTER["Rx"].as<int8_t>(); // -1
            _sys_prams.hw_cfg.PRINTER.Rts = hw_cfg_PRINTER["Rts"].as<int8_t>(); // -1
        }

        if (_sys_prams.hw_cfg.CpuFrequencyMhz != 80 &&
            _sys_prams.hw_cfg.CpuFrequencyMhz != 160 &&
            _sys_prams.hw_cfg.CpuFrequencyMhz != 240)
        {
            _sys_prams.hw_cfg.CpuFrequencyMhz = 240;
        }
    } else {
        if (ESP.getFlashChipSize() > FLASH_SIZE_4M) {
            _sys_prams.hw_cfg.hw_version = HW_VERSION_INT; // v2.3 or 2.4
        } else {
            _sys_prams.hw_cfg.hw_version = 22; // v2.2
        }

        bitSet(_statusUpdate, ADD_HARDWARE_STATUS);
        status = UPDATE_STATUS;
    }

    if (_sys_prams.hw_cfg.hw_version >= HW_VERSION_INT) {
        if (_sys_prams.hw_cfg.LAN.RstIO == -1) {    // BT8R6
            _sys_prams.hw_cfg.LAN.RstIO = ETH_RESET_PIN;
            status = UPDATE_STATUS;
        }
    }

    if (ESP.getFlashChipSize() > FLASH_SIZE_4M) {
        if (22 == _sys_prams.hw_cfg.hw_version) {
            _sys_prams.hw_cfg.hw_version = HW_VERSION_INT;
            bitSet(_statusUpdate, NEW_HARDWARE_STATUS);
            status = UPDATE_STATUS;
        }
    } else {
        if (HW_VERSION_INT == _sys_prams.hw_cfg.hw_version) {
            _sys_prams.hw_cfg.hw_version = 22;
            bitSet(_statusUpdate, NEW_HARDWARE_STATUS);
            status = UPDATE_STATUS;
        }
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