#ifndef _HTH_ESP_ETHERNET_H
#define _HTH_ESP_ETHERNET_H

#include <Arduino.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPBoard.h"

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
#ifdef ESP32
#include <ETH.h>
#elif defined(ESP8266)
#define ETH_SNTP_ENABLE 0
#if (ETH_SNTP_ENABLE)
#include <Ticker.h>
#endif
#include <SPI.h>
#include <LwipIntfDev.h>
#include <utility/enc28j60.h>

// make the same Ethernet ESP32 API
class SPIEthernet : public LwipIntfDev<ENC28J60>
{
private:
   /* data */
public:
   SPIEthernet(/* args */);
   ~SPIEthernet();
   bool linkUp();
   /**
    * Get the DNS ip address.
    * @param dns_no
    * @return IPAddress DNS Server IP
    */
   IPAddress dnsIP(uint8_t dns_no = 0) {
      return IPAddress(dns_getserver(dns_no));
   }
};

extern SPIEthernet ETH;

#endif

#ifdef ESP32
/* 
   * ETH_CLOCK_GPIO0_IN   - default: external clock from crystal oscillator
   * ETH_CLOCK_GPIO0_OUT  - 50MHz clock from internal APLL output on GPIO0 - possibly an inverter is needed for LAN8720
   * ETH_CLOCK_GPIO16_OUT - 50MHz clock from internal APLL output on GPIO16 - possibly an inverter is needed for LAN8720
   * ETH_CLOCK_GPIO17_OUT - 50MHz clock from internal APLL inverted output on GPIO17 - tested with LAN8720
*/
#ifdef ETH_CLK_MODE
#undef ETH_CLK_MODE
#endif
#define ETH_CLK_MODE    ETH_CLOCK_GPIO17_OUT

// Pin# of the enable signal for the external crystal oscillator (-1 to disable for internal APLL source)
#define ETH_POWER_PIN   -1

// Type of the Ethernet PHY (LAN8720 or TLK110)
#define ETH_TYPE        ETH_PHY_LAN8720

// I²C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
#define ETH_ADDR        1

// Pin# of the I²C clock signal for the Ethernet PHY
#define ETH_MDC_PIN     5

// Pin# of the I²C IO signal for the Ethernet PHY
#define ETH_MDIO_PIN    18
#elif defined(ESP8266)
   // nothing setting
#endif

class ESPEthernet
{
private:
   bool _status;
#ifdef ESP8266
   bool _connected;
#if (ETH_SNTP_ENABLE)
   Ticker _tickerPrintTime;
#endif
#endif
public:
   ESPEthernet(/* args */);
   ~ESPEthernet();
   bool begin();
   void enable();
   void disable();
   bool isEnable();
   void loop();
};

extern ESPEthernet Ethernet;

#endif // (defined ETH_ENABLE) && (ETH_ENABLE == 1)

#endif // _HTH_ESP_ETHERNET_H
