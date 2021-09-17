#ifndef	__APP_CONFIG_H
#define __APP_CONFIG_H

// <h> Hardware Version

//==========================================================
// <o> SERIAL_NUMBER_ADDR

#ifndef SERIAL_NUMBER_ADDR
#define SERIAL_NUMBER_ADDR          0x1FDE0
#define SERIAL_NUMBER_LENGTH_MAX    32
#endif

//==========================================================
// <o> HW_VERSION_STRING

#ifndef HW_VERSION_STRING
#define HW_VERSION_STRING "ESP8266_V1.1"
#endif

// </h>

//==========================================================
// <o> FW_VERSION_MAJOR
 
#ifndef FW_VERSION_MAJOR
#define FW_VERSION_MAJOR 1
#endif

//==========================================================
// <o> FW_VERSION_MINOR
 
#ifndef FW_VERSION_MINOR
#define FW_VERSION_MINOR 0
#endif

//==========================================================
// <o> FW_VERSION_BUILD
 
#ifndef FW_VERSION_BUILD
#define FW_VERSION_BUILD 0
#endif


#ifndef WDT_TIMEOUT_VALUE
#define WDT_TIMEOUT_VALUE  10000 /* ms */
#endif

/* DDNS client service
0: Disable
1: Enable
https://github.com/ayushsharma82/EasyDDNS
http://ipv4bot.whatismyipaddress.com/
https://github.com/me-no-dev/ESPAsyncTCP/issues/18
https://github.com/boblemaire/asyncHTTPrequest
*/
#define DDNS_CLIENT_ENABLE  1

/* mDNS Server
0: Disable
1: Enable
*/
#define MDNS_SERVICE_ENABLE  1

/* OTA Arduino
0: Disable
1: Enable
*/
#define OTA_ARDUINO_ENABLE  1

/* SNTP
0: Disable
1: Enable
*/
#define SNTP_SERVICE_ENABLE  1

/* DNS Server
0: Disable
1: Enable
*/
#define DNS_SERVER_ENABLE  1

/* NBNS service
0: Disable
1: Enable
*/
#define NBNS_SERVICE_ENABLE  1

/* LAN network
0: Disable
1: Enable
*/
#define ETH_ENABLE    0

/* Enable sd card
0: Disable
1: Enable
*/
#define SD_CARD_ENABLE  0

/* Select sd card interface
0: SD_MMC
1: SD interface over SPI
*/
#define SD_CARD_SYSTEM  1

/* Enable control power sd card 
0: not use
1: use
*/
#define SD_POWER_ENABLE 0

/* Nand memory file system
0: SPIFFS
1: LITTLEFS
2: FATFS
*/
#define USE_NAND_FS_SYSTEM 1

/* Include header fife system */
#if (defined USE_NAND_FS_SYSTEM) && (USE_NAND_FS_SYSTEM == 2)
  #define NAND_FS_SYSTEM FFat
  #include <FS.h>
  #include <FFat.h>
#elif (defined USE_NAND_FS_SYSTEM) && (USE_NAND_FS_SYSTEM == 1)
#ifdef ESP32
  #define NAND_FS_SYSTEM LITTLEFS
  #include <FS.h>
  #include <LITTLEFS.h>
#elif defined(ESP8266)
  #define NAND_FS_SYSTEM LittleFS
  #include <FS.h>
  #include <LittleFS.h>
#endif
#else
  #define NAND_FS_SYSTEM SPIFFS
  #include <SPIFFS.h>
#endif

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#include <FS.h>
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1)
#include <SD.h>
#include <SPI.h>
#else
#include <SD_MMC.h>
#endif
#endif

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
#ifdef ESP32
#include "lan8720a_cfg.h"
#elif defined(ESP8266)
#endif
#endif

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)
#include "AsyncEasyDDNS.h"
#endif

#if (defined MDNS_SERVICE_ENABLE) && (MDNS_SERVICE_ENABLE == 1)
#ifdef ESP32
#include <ESPmDNS.h>
#elif defined(ESP8266)
#include <ESP8266mDNS.h>
#endif
#endif

#if (defined NBNS_SERVICE_ENABLE) && (NBNS_SERVICE_ENABLE == 1)
#ifdef ESP32
#include <NetBIOS.h>
#elif defined(ESP8266)
#include <ESP8266NetBIOS.h>
#endif
#endif

#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 1)
#ifdef ESP8266
#include <WiFiUdp.h>
#endif
#include <ArduinoOTA.h>
#endif

#endif // __APP_CONFIG_H
