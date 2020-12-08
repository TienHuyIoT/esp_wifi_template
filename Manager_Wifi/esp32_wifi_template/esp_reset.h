#ifndef	_ESP_REBOOT_H_
#define _ESP_REBOOT_H_

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

void esp_reset_enable(void);
void esp_reboot(void);

#endif
