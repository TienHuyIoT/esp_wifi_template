#ifndef	__ESP_BOARD_H
#define __ESP_BOARD_H

#include <Arduino.h>
#include <functional>  // std::function

/* GPIO Led status
 * -1: be not used
*/

#ifndef LED_BUILTIN
#define LED_BUILTIN             22 
#endif

#define LED_STATUS_GPIO         LED_BUILTIN
#define led_status_init()       pinMode(LED_STATUS_GPIO, OUTPUT)
#define led_status_on()         digitalWrite(LED_STATUS_GPIO, LOW)
#define led_status_off()        digitalWrite(LED_STATUS_GPIO, HIGH)

/** GPIO to reset factory
 * -1: be not used
*/
#define FACTORY_INPUT_PIN       -1

/*
SPI BUS
*/
#ifdef ESP32
#define SPI_SCK_PIN     		5
#define SPI_MISO_PIN     		18
#define SPI_MOSI_PIN     		17
#define SPI_NSS_PIN				16 
#elif defined(ESP8266)
#define SPI_SCK_PIN     		14
#define SPI_MISO_PIN     		12
#define SPI_MOSI_PIN     		13
#define SPI_NSS_PIN				15

// GPIO Ethernet SPI Chip select
#define ETH_NSS_PIN             15
#endif

// GPIO SDcard SPI Chip select, don't care if the SD_CARD_ENABLE = 0
#define SD_NSS_PIN				16
/**GPIO control power of SD card, don't care if the SD_CARD_ENABLE = 0
 * -1: be not used
*/
#define SD_POWER_PIN     		-1

/** GPIO to enable ethernet, don't care if the ETH_ENABLE = 0 
 * -1: be not used
*/
#define ETH_GPIO_ENABLE         -1
#if (ETH_GPIO_ENABLE != -1)
#define ETH_GPIO_ENABLE_INIT()  pinMode(ETH_GPIO_ENABLE, INPUT_PULLUP)
#define ETH_STATUS_IS_ON()      (digitalRead(ETH_GPIO_ENABLE) == 0)
#else
#define ETH_GPIO_ENABLE_INIT()
#define ETH_STATUS_IS_ON()    
#endif

#endif
