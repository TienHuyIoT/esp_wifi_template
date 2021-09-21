#include "THIoT_ESPConfig.h"

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)

#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_SerialTrace.h"

#define ESP_ETH_PORT CONSOLE_PORT
#define ETH_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[ETH]", __VA_ARGS__)

#ifdef ESP8266
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

bool ESPEthernet::begin()
{ 
  if (!_status) 
  {
    ETH_TAG_CONSOLE("ETH disable");
    return false;
  }
  ETH_TAG_CONSOLE("ETH Start");
#ifdef ESP32
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
  ETH.begin();
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
        ETH_TAG_CONSOLE("Connected");
        ETH_TAG_CONSOLE("Ip: %s", ETH.localIP().toString().c_str());
        ETH_TAG_CONSOLE("Gw: %s", ETH.gatewayIP().toString().c_str());
        ETH_TAG_CONSOLE("Sn: %s", ETH.subnetMask().toString().c_str());
        ETH_TAG_CONSOLE("Dns: %s\r\n", ETH.dnsIP().toString().c_str());
        _connected = true;
      }
    }
    else
    {
      if (_connected)
      {
        ETH_TAG_CONSOLE("disconnect");
        _connected = false;
      }
    }
  }
#endif
}

void ESPEthernet::enable(void)
{
  ETH_TAG_CONSOLE("ETH Enable");
  _status = 1;
}

void ESPEthernet::disable(void)
{
  ETH_TAG_CONSOLE("ETH Disable");
  _status = 0;
}

bool ESPEthernet::isEnable(void)
{
  ETH_TAG_CONSOLE("status %u", _status ? "ON" : "OFF");
  return _status;
}

ESPEthernet Ethernet;

#endif