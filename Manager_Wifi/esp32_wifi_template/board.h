#ifndef	_BOARD_H
#define _BOARD_H

#define LED_STATUS_GPIO         3 /* 3(web485), 23(ken_sos) */
#define led_status_init()       pinMode(LED_STATUS_GPIO, OUTPUT)
void led_status_toggle()        {digitalWrite(LED_STATUS_GPIO, !digitalRead(LED_STATUS_GPIO));}
#define led_status_on()         digitalWrite(LED_STATUS_GPIO, LOW)
#define led_status_off()        digitalWrite(LED_STATUS_GPIO, HIGH)

/*
SPI BUS
*/
#define SPI_SCK_PIN     		5
#define SPI_MISO_PIN     		18
#define SPI_MOSI_PIN     		17

/* GPIO to reset factory */
#define FACTORY_INPUT_PIN       36 /* Button1 36(web485), -1(ken_sos) */

typedef void (*led_callback_t)(int);

#endif
