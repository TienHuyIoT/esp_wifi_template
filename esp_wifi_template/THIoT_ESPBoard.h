#ifndef	__ESP_BOARD_H
#define __ESP_BOARD_H

#define LED_STATUS_GPIO         25 /* HW 25(Heltec) 3(web485), 23(ken_sos), 2(esp8266) */

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
// GPIO control power of SD card, don't care if the SD_CARD_ENABLE = 0
#define SD_POWER_PIN     		-1

/* GPIO to reset factory */
#define FACTORY_INPUT_PIN       0 /* HW 36(web485 Button1), -1(ken_sos), 0(esp8266) */

/* GPIO to enable ethernet, don't care if the ETH_ENABLE = 0 */
#define ETH_GPIO_ENABLE         39 /* HW 39(web485 Button2), 4(esp8266)*/
#if (ETH_GPIO_ENABLE != -1)
#define ETH_GPIO_ENABLE_INIT()  pinMode(ETH_GPIO_ENABLE, INPUT_PULLUP)
#define ETH_STATUS_IS_ON()      (digitalRead(ETH_GPIO_ENABLE) == 0)
#else
#define ETH_GPIO_ENABLE_INIT()
#define ETH_STATUS_IS_ON()    
#endif

#endif // __ESP_BOARD_H
