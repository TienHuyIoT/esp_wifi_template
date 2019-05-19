#ifndef	_FILE_CONFIG_H
#define _FILE_CONFIG_H

#include <stdint.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <FS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <SPIFFS.h>
#endif

extern const char *HwVersion;
extern const char *FwVersion;

#ifdef ESP8266
void ListDir(void);
#elif defined(ESP32)
void ListDir(fs::FS &fs, const char * dirname, uint8_t levels);
#endif

/*=================================================================
 Debug FS
=================================================================*/
#ifdef ESP8266
#define DBG_FS_PORT Serial
#elif defined(ESP32)
#define DBG_FS_PORT Serial
#endif
#define DEBUGLOG_FS(fmt, ...) DBG_FS_PORT.printf_P(PSTR("\r\n>FS< " fmt), ##__VA_ARGS__)
/*=================================================================
 Debug EEPROM
=================================================================*/
#ifdef ESP8266
#define DBG_EEPROM_PORT Serial
#elif defined(ESP32)
#define DBG_EEPROM_PORT Serial
#endif
#define DEBUGLOG_EEPROM(fmt, ...) DBG_EEPROM_PORT.printf_P(PSTR("\r\n>Eeprom< " fmt), ##__VA_ARGS__)

typedef enum {
  P_NULL  = (uint8_t)0, /*!< No Page*/
  P_HOME  = (uint8_t)1, /*!< Page home.htm display*/
  P_INDEX = (uint8_t)2, /*!< Page homt.htm Fn*/
} PAGE_TAG_TypeDef;

#define Df_LengHostName       32
#define Df_LengSsid           32
#define Df_LengPass           64
#define Df_LengAuth           10
#define Df_LengAddr           100
#define Df_LengServer         32
typedef struct{
	uint16_t 		UDPPort;      /*UDP Port*/
	uint16_t 		TCPPort;      /*TCP Port*/
  uint16_t    WSPort;       /*Websocket Port*/
  struct{
    char    Name[Df_LengAddr + 1]; 
    char    Addr[Df_LengAddr + 1]; 
  }ADDR;
  struct{
    char    user[Df_LengAuth + 1]; 
    char    pass[Df_LengAuth + 1]; 
  }AUTH;
	struct{		
		char 		ssid[Df_LengSsid + 1];
		char 		pass[Df_LengPass + 1];
    char    HostName[Df_LengHostName+1];
		uint8_t 	SaticIp;
		IPAddress 	Ip;
		IPAddress 	Gw;
		IPAddress 	Sn;
		IPAddress 	Dns;
	}STA;
	struct{		
		char 		ssid[Df_LengSsid + 1];
		char 		pass[Df_LengPass + 1];
    char    DnsName[Df_LengHostName+1];
    IPAddress   Ip;
    IPAddress   Sn;
		uint8_t 	Chanel : 4;
		uint8_t 	Hidden : 1;  
	}AP;	
}CONFIGFILE;

#define Df_ReadConfig   	0
#define Df_UpdateConfig  	1
#define ParthConfig		"/config.txt"

#define Df_LengMacAuth          10
typedef struct{ 
  struct{   
    char    Auth[Df_LengMacAuth + 1];
  }MAC;  
}CONFIGFILEHIDDEN;

#define Df_ReadConfigHidden     0
#define Df_UpdateConfigHidden   1
#define ParthConfigHidden   "/confighidden.txt"

typedef struct{  
  union{
    uint32_t Block0;
    struct{
      uint8_t AutoConfig;
      uint8_t Activated;
    };
  }; 
  union{
    uint32_t Block1_2[2];
    struct{
      byte mac[6];  
    };
  }; 
}EEPSTR;

#define Df_MacUnActivate     0
#define Df_MacActivated      1

#define Df_ReadEep     0
#define Df_UpdateEep   1

#define Df_ConfigActiveRead       0
#define Df_ConfigActiveUpdate     1
#define Df_ConfigUnActiveUpdate   2

class FileSystemConfig
{
  private:

  public:
    FileSystemConfig();
    EEPSTR EepStr;
    CONFIGFILE ConfigFile;
    CONFIGFILEHIDDEN ConfigFileHidden;
    uint8_t ActivateByMac;
    String formatBytes(size_t bytes);
    void FsFormat(void);
    void ConfigActivated(uint8_t Cmd);
    void Eepromfactory(void);
    void EepromConfig(uint8_t Cmd);
    void FS_FileConfigHidden(uint8_t Cmd);
    void FS_FileConfig(uint8_t Cmd);
    void ShowInfo(void);
    #ifdef ESP8266
    void ListDir(void);
    #elif defined(ESP32)
    void ListDir(fs::FS &fs, const char * dirname, uint8_t levels);
    #endif
};

extern FileSystemConfig FileConfig;
#endif




