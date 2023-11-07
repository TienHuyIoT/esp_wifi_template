#include "THIoT_ESPConfig.h"

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)

#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPEthernet.h"
#include "THIoT_ESPAsyncEasyNTP.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_PFLedStatus.h"
#include "THIoT_ESPSoftReset.h"

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
  _enable = false;
#ifdef ESP8266
  _connected = false;
#endif
}

ESPEthernet::~ESPEthernet()
{
}

ETHLedStatusHandler ESPEthernet::_ledStatusFunc = nullptr;
boolean ESPEthernet::_IsConnected = false;

bool ESPEthernet::begin()
{ 
  if (!_enable) {
    ETH_TAG_CONSOLE("ETH was disable");
    return false;
  }
  ETH_TAG_CONSOLE("ETH Start");
  this->disconnectEvt(); /* Purpose: active timeout connection reset */
#ifdef ESP32
#if (LAN_ENC28J60 == 1) || (LAN_W5500 == 1)
  ETH.begin( MISO_GPIO, MOSI_GPIO, SCLK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, SPI_HOST);
#else
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
#endif
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

#ifdef ESP8266
void ESPEthernet::loop() {
  if (_enable) {
    if (ETH.connected()) {
      if (!_connected) {
        _connected = true;
        ETH_TAG_CONSOLE("connected");
        ETH_TAG_LOG("connected, got IP is %s", ETH.localIP().toString().c_str());
        ETH_TAG_CONSOLE("Ip: %s", ETH.localIP().toString().c_str());
        ETH_TAG_CONSOLE("Gw: %s", ETH.gatewayIP().toString().c_str());
        ETH_TAG_CONSOLE("Sn: %s", ETH.subnetMask().toString().c_str());
        ETH_TAG_CONSOLE("Dns: %s\r\n", ETH.dnsIP().toString().c_str());
        this->connectedEvt();
        /* Run DDNS service */
        EasyDDNS.updateTrigger();
      }
    }
    else {
      if (_connected) {
        _connected = false;
        this->disconnectEvt();
      }
    }
  }
}
#endif

boolean ESPEthernet::disconnectEvt() { 
  _IsConnected = false;
  ETH_TAG_CONSOLE("[EVENT] disconnect");
  ETH_TAG_LOG("[EVENT] disconnect");
  if (_ledStatusFunc) {
    _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_ETH_DISCONNECTED);
  }

#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
  EASY_NTP.end();
#endif

#if (NETWORK_CONNECTION_TIMEOUT_RESET == 1)
  ETH_TAG_CONSOLE("[EVENT] Enable timeout connection reset");
  ticker_once(&_reconnectTicker, ETH_TIMEOUT_CONNECTION_RESET, [](void *arg) {
    ESPEthernet *handler = (ESPEthernet*)arg;
    ETH_TAG_CONSOLE("[EVENT] Timeout connection reset");
    SOFTReset.enable(500, ESPSoftReset::WIFI_RECONNECT_TYPE);
  }, this);
#endif
}

boolean ESPEthernet::connectedEvt() { 
  _IsConnected = true; 
#ifdef ESP32
  ETH_TAG_LOG("[EVENT] connected");
  ETH_TAG_CONSOLE("[EVENT] MAC: %s, IPv4: %s", ETH.macAddress().c_str(), ETH.localIP().toString().c_str());
  if (ETH.fullDuplex()) {
      ETH_TAG_CONSOLE("[EVENT] FULL_DUPLEX");
  }
  ETH_TAG_CONSOLE("[EVENT] Link Speed %uMbps", ETH.linkSpeed()); 
#endif
  if (_ledStatusFunc) {
    _ledStatusFunc(ESPLedCycleBlinkCallbacks::BLINK_ETH_GOT_IP);
  }

#if (defined ASYNC_EASY_SNTP) && (ASYNC_EASY_SNTP == 1)
#if (TIME_ZONE_TYPE_CFG == 1)
  EASY_NTP.begin(ESPConfig.gmtOffsetSNTP(), ESPConfig.daylightOffsetSNTP(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
#else
  EASY_NTP.begin(ESPConfig.TzTime().c_str(), ESPConfig.server1SNTP().c_str(), ESPConfig.intervalSNTP());
#endif
#endif

#if (NETWORK_CONNECTION_TIMEOUT_RESET == 1)
  ETH_TAG_CONSOLE("[EVENT] Disable timeout connection reset");
  ticker_detach(&_reconnectTicker);
#endif
}

void ESPEthernet::enable(void) {
  ETH_TAG_CONSOLE("ETH set Enable");
  _enable = true;
}

void ESPEthernet::disable(void)
{
  ETH_TAG_CONSOLE("ETH set Disable");
  _enable = false;
}

boolean ESPEthernet::isEnable(void)
{
  ETH_TAG_CONSOLE("status %s", _enable ? "ON" : "OFF");
  return _enable;
}

ESPEthernet Ethernet;

#endif