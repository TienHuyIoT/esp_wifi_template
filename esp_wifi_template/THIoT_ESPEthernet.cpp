#include "THIoT_ESPConfig.h"

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)

#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_ESPAsyncEasyNTP.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_SerialTrace.h"
#include "esp_led_status.h"

#define ESP_ETH_PORT SERIAL_PORT
#define ETH_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[ETH]", __VA_ARGS__)
#define ETH_TAG_LOG(...) FS_TAG_LOGI("[ETH]", __VA_ARGS__)

#ifdef EPS32
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiType.h>
#elif defined(ESP8266)
#include <lwip/apps/sntp.h>
SPIEthernet::SPIEthernet(/* args */)
:LwipIntfDev(ETH_NSS_PIN)
{
}

SPIEthernet::~SPIEthernet()
{
}

bool SPIEthernet::linkUp()
{
  return (status() == WL_CONNECTED);
}

SPIEthernet ETH;
#endif

ESPEthernet::ESPEthernet(/* args */)
{
  _status = false;
#ifdef ESP8266
  _connected = false;
#endif
}

ESPEthernet::~ESPEthernet()
{
}

ETHLedStatusHandler ESPEthernet::_ledStatusFunc = nullptr;

bool ESPEthernet::begin()
{ 
  if (!_status) 
  {
    ETH_TAG_CONSOLE("ETH was disable");
    return false;
  }
  ETH_TAG_CONSOLE("ETH Start");
#ifdef ESP32
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
  {
    ETH_TAG_CONSOLE("started");
    ETH_TAG_LOG("started");
    String ChipID = String((uint32_t)(ESP.getEfuseMac() >> 16), HEX);
    ChipID.toUpperCase();
    String hostName = "ETH_" + ESPConfig.hostNameSTA() + "_" + ChipID;
    ETH.setHostname(hostName.c_str());
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_START);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
  {
    ETH_TAG_CONSOLE("stopped");
    ETH_TAG_LOG("stopped");
    if (_ledStatusFunc)
    {
      _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_ETH_DISCONNECTED);
    }
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_STOP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
  {
    ETH_TAG_CONSOLE("[EVENT] got IP address: %s", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
    ETH_TAG_CONSOLE("MAC: %s, IPv4: %s", ETH.macAddress().c_str(), ETH.localIP().toString().c_str());
    ETH_TAG_LOG("[EVENT] got IP address: %s", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
    if (ETH.fullDuplex()) {
        ETH_TAG_CONSOLE("FULL_DUPLEX");
    }
    ETH_TAG_CONSOLE("Link Speed %uMbps", ETH.linkSpeed()); 
    if (_ledStatusFunc)
    {
      _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_ETH_GOT_IP);
    }
#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
    ETH_TAG_LOG("[EASYNTP] begin(%ld, %d, %s, %u)"
          , ESPConfig.gmtOffsetSNTP() * 3600, ESPConfig.daylightOffsetSNTP(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
    EASYNTP.begin(ESPConfig.gmtOffsetSNTP() * 3600, ESPConfig.daylightOffsetSNTP(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
#endif
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
  {
    ETH_TAG_CONSOLE("connected");
    ETH_TAG_LOG("connected");
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_CONNECTED);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
  {
    ETH_TAG_CONSOLE("disconnected");
    ETH_TAG_LOG("disconnected");
    if (_ledStatusFunc)
    {
      _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_ETH_DISCONNECTED);
    }
#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
    EASYNTP.end();
#endif
  }
  ,WiFiEvent_t::m_ESP32_EVENT_ETH_DISCONNECTED);

  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
#endif
  if (!ESPConfig.dhcpSTA())
  {
    /* Config must be after begin function for ESP32 */
    ETH.config(ESPConfig.ipSTA(), ESPConfig.gwSTA(), ESPConfig.snSTA(), ESPConfig.dnsSTA());
    ETH_TAG_CONSOLE("static IP enable");
    ETH_TAG_CONSOLE("Ip: %s", ESPConfig.ipSTA().toString().c_str());
    ETH_TAG_CONSOLE("Gw: %s", ESPConfig.gwSTA().toString().c_str());
    ETH_TAG_CONSOLE("Sn: %s", ESPConfig.snSTA().toString().c_str());
    ETH_TAG_CONSOLE("Dns: %s\r\n", ESPConfig.dnsSTA().toString().c_str());
  }
#ifdef ESP8266
  /* Config must be after config function for ESP8266 */
  ETH.begin();
  String ChipID = String(ESP.getChipId(), HEX);
  ChipID.toUpperCase();
  String hostName = "ETH_" + ESPConfig.hostNameSTA() + "_" + ChipID;
  ETH_TAG_CONSOLE("SetHostname: %s", hostName.c_str());
  WiFi.setHostname(hostName.c_str());
#endif
  return true;
}

void ESPEthernet::loop()
{
#ifdef ESP8266
  if (_status)
  {
    if (ETH.connected())
    {
      if (!_connected)
      {
        ETH_TAG_CONSOLE("connected");
        ETH_TAG_LOG("connected, got IP is %s", ETH.localIP().toString().c_str());
        ETH_TAG_CONSOLE("Ip: %s", ETH.localIP().toString().c_str());
        ETH_TAG_CONSOLE("Gw: %s", ETH.gatewayIP().toString().c_str());
        ETH_TAG_CONSOLE("Sn: %s", ETH.subnetMask().toString().c_str());
        ETH_TAG_CONSOLE("Dns: %s\r\n", ETH.dnsIP().toString().c_str());
        _connected = true;
        if (_ledStatusFunc)
        {
          _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_ETH_GOT_IP);
        }
#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
        ETH_TAG_LOG("[EASYNTP] begin(%ld, %d, %s, %u)"
          , ESPConfig.gmtOffsetSNTP() * 3600, ESPConfig.daylightOffsetSNTP(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
        EASYNTP.begin(ESPConfig.gmtOffsetSNTP() * 3600, ESPConfig.daylightOffsetSNTP(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
#endif
      }
    }
    else
    {
      if (_connected)
      {
        ETH_TAG_CONSOLE("disconnect");
        ETH_TAG_LOG("disconnect");
        _connected = false;
        if (_ledStatusFunc)
        {
          _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_ETH_DISCONNECTED);
        }
#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
        EASYNTP.end();
#endif
      }
    }
  }
#endif
}

void ESPEthernet::enable(void)
{
  ETH_TAG_CONSOLE("ETH set Enable");
  _status = 1;
}

void ESPEthernet::disable(void)
{
  ETH_TAG_CONSOLE("ETH set Disable");
  _status = 0;
}

bool ESPEthernet::isEnable(void)
{
  ETH_TAG_CONSOLE("status %u", _status ? "ON" : "OFF");
  return _status;
}

ESPEthernet Ethernet;

#endif