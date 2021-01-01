#ifndef	_ESP_REBOOT_H_
#define _ESP_REBOOT_H_

//flag to use from web update to reboot the ESP
bool esp_should_reboot = false;
uint32_t esp_reboot_timeout;

void esp_reset_enable(void);
void esp_reboot_handle(void);

#endif
