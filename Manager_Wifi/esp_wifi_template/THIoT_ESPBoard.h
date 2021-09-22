#ifndef	__ESP_BOARD_H
#define __ESP_BOARD_H

#define LED_STATUS_GPIO         2 /* 3(web485), 23(ken_sos) */
#define led_status_init()       pinMode(LED_STATUS_GPIO, OUTPUT)
#define led_status_toggle()     {digitalWrite(LED_STATUS_GPIO, !digitalRead(LED_STATUS_GPIO));}
#define led_status_on()         digitalWrite(LED_STATUS_GPIO, LOW)
#define led_status_off()        digitalWrite(LED_STATUS_GPIO, HIGH)

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

// GPIO SDcard SPI Chip select
#define SD_NSS_PIN				16 
// GPIO control power of SD card
#define SD_POWER_PIN     		5

/* GPIO to reset factory */
#define FACTORY_INPUT_PIN       0 /* Button1 36(web485), -1(ken_sos) */

/* GPIO to enable ethernet */
#define ETH_GPIO_ENABLE         4 /* Button2 */
#if (ETH_GPIO_ENABLE != -1)
#define ETH_GPIO_ENABLE_INIT()  pinMode(ETH_GPIO_ENABLE, INPUT_PULLUP)
#define ETH_STATUS_IS_ON()      (digitalRead(ETH_GPIO_ENABLE) == 0)
#else
#define ETH_GPIO_ENABLE_INIT()
#define ETH_STATUS_IS_ON()    
#endif

#endif // __ESP_BOARD_H
