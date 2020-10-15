#include <EEPROM.h>
#include <ArduinoJson.h>
#include "FileConfig.h"

const char *HwVersion = "1.0";
const char *FwVersion = "1.0.0";

#define Df_NumFileOriginal  10
const char* const FilsOriginal[Df_NumFileOriginal] PROGMEM = {
  "/config.txt",           //1
  "/confighidden.txt",     //2
  "/edit.htm.gz",          //3
  "/graphs.js.gz",         //4
  "/favicon.ico",          //5
  "/home.htm",             //6
  "/update.htm",           //7
  "/upload.htm",           //8
  "/changepass.htm",       //9
  "/Reset.htm"            //10
};

const char ConfigFileInit[] PROGMEM = 
"{\"NameDev\":\"NAVIS_LORA\","
"\n\"AddrDev\":\"503/5/18A Pham Van Bach\","
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
"\n\"STASsid\":\"Nhatrang Hitech\","
"\n\"STAPass\":\"88888888\","
"\n\"STAName\":\"nhtc\","
"\n\"APSsid\":\"DTUD\","
"\n\"APPass\":\"nhtc130nmh\","
"\n\"APName\":\"nhtc.com.vn\","
"\n\"APIp\":\"192.168.4.1\","
"\n\"APSn\":\"255.255.255.0\","
"\n\"Chanel\":1,"
"\n\"Hidden\":0}";

const char ConfigHiddenFileInit[] PROGMEM=
"{\"MacAuth\":\"1234\","
"\n\"NC\":0}";

FileSystemConfig::FileSystemConfig(){}

//format bytes
String FileSystemConfig::formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

void FileSystemConfig::FsFormat(void) {
  uint8_t i;
#ifdef ESP8266
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    DEBUGLOG_FS("File: %s, size: %s", fileName.c_str(), formatBytes(fileSize).c_str());
    for (i = 0; i < Df_NumFileOriginal; i++) {
      if (!strcmp_P(fileName.c_str(), FilsOriginal[i])){
        DEBUGLOG_FS("Exsit");
        break;
      }
    }
    if (i == Df_NumFileOriginal) {
      //Xoa file
      SPIFFS.remove(fileName);
      DEBUGLOG_FS("Remove");
    }
  }
  DEBUGLOG_FS("End\r\n");
#elif defined(ESP32)
  File root = SPIFFS.open("/");
  if (!root) {
    DEBUGLOG_FS("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    DEBUGLOG_FS("Not a directory");
    root.close();
    return;
  }
  root.rewindDirectory();

  File file = root.openNextFile();

  while (file) {
    String fileName = file.name();
    size_t fileSize = file.size();
    DEBUGLOG_FS("File: %s, size: %s", fileName.c_str(), formatBytes(fileSize).c_str());
    for (i = 0; i < Df_NumFileOriginal; i++) {
      if (!strcmp(fileName.c_str(), FilsOriginal[i])){
        DEBUGLOG_FS("Exsit");
        break;
      }
    }
    if (i == Df_NumFileOriginal) {
      //Xoa file
      SPIFFS.remove(fileName);
      DEBUGLOG_FS("Remove");
    }
    file = root.openNextFile();
  }
  root.close();
  DEBUGLOG_FS("End\r\n");
#endif
}

#ifdef ESP8266
void FileSystemConfig::ListDir(void){
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    DEBUGLOG_FS("FS File: %s, size: %s", fileName.c_str(), formatBytes(fileSize).c_str());
  }
}
#elif defined(ESP32)
void FileSystemConfig::ListDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  DEBUGLOG_FS("Listing directory: %s", dirname);

  File root = fs.open(dirname);
  if (!root) {
    DEBUGLOG_FS("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    DEBUGLOG_FS("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      DEBUGLOG_FS("DIR : %s",file.name());
      if (levels) {
        ListDir(fs, file.name(), levels - 1);
      }
    } else {
      DEBUGLOG_FS("FILE: %s",file.name());
      DEBUGLOG_FS("SIZE: %u",file.size());
    }
    file = root.openNextFile();
  }
}
#endif

void FileSystemConfig::FS_FileConfig(uint8_t Cmd) {
  File Fs_Config;
  if (!SPIFFS.exists(ParthConfig)) {    
    Fs_Config = SPIFFS.open(ParthConfig, "w");
    Fs_Config.printf_P(ConfigFileInit);
    Fs_Config.close();
  }

  if (Cmd == Df_UpdateConfig) {
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();
    root["NameDev"] = ConfigFile.ADDR.Name;
    root["AddrDev"] = ConfigFile.ADDR.Addr;
    root["UDPPort"] = ConfigFile.UDPPort;
    root["TCPPort"] = ConfigFile.TCPPort;
    root["WSPort"] = ConfigFile.WSPort;
    root["AuthUser"] = ConfigFile.AUTH.user;
    root["AuthPass"] = ConfigFile.AUTH.pass;
    root["SaticIp"] = ConfigFile.STA.SaticIp;
    root["Ip"] = ConfigFile.STA.Ip.toString();
    root["Gw"] = ConfigFile.STA.Gw.toString();
    root["Sn"] = ConfigFile.STA.Sn.toString();
    root["Dns"] = ConfigFile.STA.Dns.toString();
    root["STASsid"] = ConfigFile.STA.ssid;
    root["STAPass"] = ConfigFile.STA.pass;
    root["STAName"] = ConfigFile.STA.HostName;
    root["APSsid"] = ConfigFile.AP.ssid;
    root["APPass"] = ConfigFile.AP.pass;
    root["APName"] = ConfigFile.AP.DnsName;
    root["APIp"] = ConfigFile.AP.Ip.toString();
    root["APSn"] = ConfigFile.AP.Sn.toString();
    root["Chanel"] = ConfigFile.AP.Chanel;
    root["Hidden"] = ConfigFile.AP.Hidden;    
    
    Fs_Config = SPIFFS.open(ParthConfig, "w");
    root.prettyPrintTo(Fs_Config);
    Fs_Config.close();    
    return;
  }

  Fs_Config = SPIFFS.open(ParthConfig, "r");
  size_t size = Fs_Config.size();
  std::unique_ptr<char[]> ConfigBuf(new char[size+1]);
  Fs_Config.readBytes(ConfigBuf.get(),size);
  Fs_Config.close();

  DynamicJsonBuffer djbpo(size + 1);
  JsonObject& root = djbpo.parseObject(ConfigBuf.get());
  if (!root.success()) {
    DEBUGLOG_FS("JSON parsing failed!");
    return;
  }
  DEBUGLOG_FS("JSON parsing OK!");
  strncpy(ConfigFile.ADDR.Name,root["NameDev"],Df_LengAddr); 
  strncpy(ConfigFile.ADDR.Addr,root["AddrDev"],Df_LengAddr);
	ConfigFile.UDPPort = root["UDPPort"];
	ConfigFile.TCPPort = root["TCPPort"];
  ConfigFile.WSPort = root["WSPort"];
  strncpy(ConfigFile.AUTH.user,root["AuthUser"],Df_LengAuth); 
  strncpy(ConfigFile.AUTH.pass,root["AuthPass"],Df_LengAuth);  
	ConfigFile.STA.SaticIp = root["SaticIp"];
	
	ConfigFile.STA.Ip.fromString(root["Ip"].as<String>());
  ConfigFile.STA.Gw.fromString(root["Gw"].as<String>());
  ConfigFile.STA.Sn.fromString(root["Sn"].as<String>());
  ConfigFile.STA.Dns.fromString(root["Dns"].as<String>());
	
	strncpy(ConfigFile.STA.ssid,root["STASsid"],Df_LengSsid); 
	strncpy(ConfigFile.STA.pass,root["STAPass"],Df_LengPass); 
  strncpy(ConfigFile.STA.HostName,root["STAName"],Df_LengHostName); 
	strncpy(ConfigFile.AP.ssid,root["APSsid"],Df_LengSsid); 

	strncpy(ConfigFile.AP.pass,root["APPass"],Df_LengPass);
  strncpy(ConfigFile.AP.DnsName,root["APName"],Df_LengHostName); 
  ConfigFile.AP.Ip.fromString(root["APIp"].as<String>());
  ConfigFile.AP.Sn.fromString(root["APSn"].as<String>());
	ConfigFile.AP.Chanel = root["Chanel"];
	ConfigFile.AP.Hidden = root["Hidden"];  	 
}

void FileSystemConfig::ShowInfo(void){
  
}

void FileSystemConfig::FS_FileConfigHidden(uint8_t Cmd) {
  File Fs_Config;
  if (!SPIFFS.exists(ParthConfigHidden)) {    
    Fs_Config = SPIFFS.open(ParthConfigHidden, "w");
    Fs_Config.printf_P(ConfigHiddenFileInit);
    Fs_Config.close();
  }

  if (Cmd == Df_UpdateConfigHidden) {
    DynamicJsonBuffer djbco;
    JsonObject& root = djbco.createObject();
    root["MacAuth"] = ConfigFileHidden.MAC.Auth;
    root["NC"] = 0;     
    Fs_Config = SPIFFS.open(ParthConfigHidden, "w");
    root.prettyPrintTo(Fs_Config);
    Fs_Config.close();    
    return;
  }

  Fs_Config = SPIFFS.open(ParthConfigHidden, "r");
  size_t size = Fs_Config.size();
  std::unique_ptr<char[]> ConfigBuf(new char[size+1]);
  Fs_Config.readBytes(ConfigBuf.get(),size);
  Fs_Config.close();

  DynamicJsonBuffer djbpo(size + 1);
  JsonObject& root = djbpo.parseObject(ConfigBuf.get());
  if (!root.success()) {
    DEBUGLOG_FS("JSON parsing failed!");
    return;
  }
  strncpy(ConfigFileHidden.MAC.Auth,root["MacAuth"],Df_LengMacAuth);    
}

void FileSystemConfig::Eepromfactory(void){  
  DEBUGLOG_EEPROM("Factory OK");
}

void FileSystemConfig::EepromConfig(uint8_t Cmd) {
  EEPROM.begin(512);
  if(Cmd == Df_ReadEep) EEPROM.get(0, EepStr);
  if(Cmd == Df_UpdateEep) EEPROM.put(0, EepStr);
  DEBUGLOG_EEPROM("Eeprom.AutoConfig: %u", EepStr.AutoConfig);
  DEBUGLOG_EEPROM("Eeprom.Activated: %u", EepStr.Activated);
  if(EepStr.AutoConfig != 129){
    EepStr.AutoConfig = 129;
    Eepromfactory();
    EEPROM.put(0, EepStr);
  }  
  EEPROM.end();
}

void FileSystemConfig::ConfigActivated(uint8_t Cmd){
  byte mac[6];
  WiFi.macAddress(mac);
  DEBUGLOG_EEPROM("Mac: %02X%02X%02X%02X%02X%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  if(Cmd==Df_ConfigActiveUpdate) memcpy(EepStr.mac,mac,sizeof(EepStr.mac));
  if(Cmd==Df_ConfigUnActiveUpdate) memset(EepStr.mac,0xff,sizeof(EepStr.mac));
  //Nếu địa chỉ Mac không được activated
  if(memcmp(EepStr.mac,mac,sizeof(EepStr.mac))) ActivateByMac = Df_MacUnActivate;
  else ActivateByMac = Df_MacActivated;
  DEBUGLOG_EEPROM("ActivateByMac: %u", ActivateByMac);
  if(Cmd != Df_ConfigActiveRead) EepromConfig(Df_UpdateEep);
}

FileSystemConfig FileConfig;




