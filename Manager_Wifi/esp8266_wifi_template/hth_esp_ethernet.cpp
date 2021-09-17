#include "app_config.h"

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)

#include "hth_esp_sys_data.h"
#include "hth_esp_ethernet.h"
#include "hth_console_dbg.h"

#define ESP_ETH_PORT CONSOLE_PORT
#define ETH_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[ETH]", __VA_ARGS__)

#ifdef ESP8266
spi_ethernet::spi_ethernet(/* args */)
:LwipIntfDev(ETH_NSS_PIN)
{
}

spi_ethernet::~spi_ethernet()
{
}

bool spi_ethernet::linkUp()
{
  return (status() == WL_CONNECTED);
}

spi_ethernet ETH;
#endif

hth_esp_ethernet::hth_esp_ethernet(/* args */)
{
  _status = false;
}

hth_esp_ethernet::~hth_esp_ethernet()
{
}

bool hth_esp_ethernet::start()
{ 
  if (_status) 
  {
    ETH_TAG_CONSOLE("ETH disable");
    return false;
  }
  ETH_TAG_CONSOLE("ETH Start");
#ifdef ESP32
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
#elif defined(ESP8266)
  ETH.begin();
#endif
  if (!WFDataFile.dhcpSTA())
  {
    /* Config must be after begin function */
    ETH.config(WFDataFile.ipSTA(), WFDataFile.gwSTA(), WFDataFile.snSTA(), WFDataFile.dnsSTA());
    ETH_TAG_CONSOLE("static IP enable");
    ETH_TAG_CONSOLE("Ip: %s", WFDataFile.ipSTA().toString().c_str());
    ETH_TAG_CONSOLE("Gw: %s", WFDataFile.gwSTA().toString().c_str());
    ETH_TAG_CONSOLE("Sn: %s", WFDataFile.snSTA().toString().c_str());
    ETH_TAG_CONSOLE("Dns: %s\r\n", WFDataFile.dnsSTA().toString().c_str());
  }
  return true;
}

void hth_esp_ethernet::enable(void)
{
  ETH_TAG_CONSOLE("ETH Enable");
  _status = 1;
}

void hth_esp_ethernet::disable(void)
{
  ETH_TAG_CONSOLE("ETH Disable");
  _status = 0;
}

bool hth_esp_ethernet::isEnable(void)
{
  ETH_TAG_CONSOLE("status %u", _status);
  return _status;
}

hth_esp_ethernet HTH_ethernet;

#endif