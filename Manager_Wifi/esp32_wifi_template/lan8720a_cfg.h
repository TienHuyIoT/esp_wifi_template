#ifndef	_LAN8720A_CONFIG_
#define _LAN8720A_CONFIG_

#include <ETH.h>
#include "board.h"

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

/* GPIO to enable ethernet */
#define ETH_GPIO_ENABLE 39
#define ETH_GPIO_ENABLE_INIT()    pinMode(ETH_GPIO_ENABLE, INPUT_PULLUP)
#define ETH_ENABLE_STATUS()       digitalRead(ETH_GPIO_ENABLE)

#endif