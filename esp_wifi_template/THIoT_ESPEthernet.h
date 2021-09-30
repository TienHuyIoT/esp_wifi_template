#ifndef _HTH_ESP_ETHERNET_H
#define _HTH_ESP_ETHERNET_H

#include <Arduino.h>
#include <functional> // std::function
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPBoard.h"
#include "THIoT_ESPEventSignal.h"

typedef std::function<void(int type)> ETHLedStatusHandler;

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
#ifdef ESP32
#include <ETH.h>
/* 
   * ETH_CLOCK_GPIO0_IN   - default: external clock from crystal oscillator
   * ETH_CLOCK_GPIO0_OUT  - 50MHz clock from internal APLL output on GPIO0 - possibly an inverter is needed for LAN8720
   * ETH_CLOCK_GPIO16_OUT - 50MHz clock from internal APLL output on GPIO16 - possibly an inverter is needed for LAN8720
   * ETH_CLOCK_GPIO17_OUT - 50MHz clock from internal APLL inverted output on GPIO17 - tested with LAN8720
*/
#ifdef ETH_CLK_MODE
#undef ETH_CLK_MODE
#endif
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT

// Pin# of the enable signal for the external crystal oscillator (-1 to disable for internal APLL source)
#define ETH_POWER_PIN -1

// Type of the Ethernet PHY (LAN8720 or TLK110)
#if (LAN_LAN8720 == 1)
#define ETH_TYPE ETH_PHY_LAN8720
// I²C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
#define ETH_ADDR 1
#elif (LAN_TLK110 == 1)
#define ETH_TYPE ETH_PHY_TLK110
// I²C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
#define ETH_ADDR 31
#else
#error "No Ethernet card selected"
#endif

// Pin# of the I²C clock signal for the Ethernet PHY
#define ETH_MDC_PIN 5

// Pin# of the I²C IO signal for the Ethernet PHY
#define ETH_MDIO_PIN 18
#elif defined(ESP8266) // make the same Ethernet ESP32 API
#include <SPI.h>
#include <LwipIntfDev.h>
#if (LAN_ENC28J60 == 1)
#include <ENC28J60lwIP.h>
class SPIEthernet : public LwipIntfDev<ENC28J60>
#elif (LAN_W5100 == 1)
#include <W5100lwIP.h>
class SPIEthernet : public LwipIntfDev<Wiznet5100>
#elif (LAN_W5500 == 1)
#include <W5500lwIP.h>
class SPIEthernet : public LwipIntfDev<Wiznet5500>
#else
#error "No Ethernet card selected"
#endif
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
    IPAddress dnsIP(uint8_t dns_no = 0)
    {
        return IPAddress(dns_getserver(dns_no));
    }
};

extern SPIEthernet ETH;
#endif

class ESPEthernet
{
private:
    bool _status;
#ifdef ESP8266
    bool _connected;
#endif
    static ETHLedStatusHandler _ledStatusFunc;

public:
    ESPEthernet(/* args */);
    ~ESPEthernet();
    bool begin();
    void enable();
    void disable();
    bool isEnable();
    void loop();
    void onLedStatus(ETHLedStatusHandler handler)
    {
        _ledStatusFunc = handler;
    }
};

extern ESPEthernet Ethernet;

#endif // (defined ETH_ENABLE) && (ETH_ENABLE == 1)

#endif // _HTH_ESP_ETHERNET_H
