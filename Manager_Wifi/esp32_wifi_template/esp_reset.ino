#include "esp_reset.h"

// need non block function
void esp_reset_enable(uint32_t timeout)
{
    esp_should_reboot = 1;
    esp_reboot_timeout = timeout;
}

void esp_reboot_handle(void)
{
    if (esp_should_reboot)
    {
        // save rtc current into fs before restart
        rtc_time_t rtc;
        rtc_get(&rtc);
        rtc_info_write(&rtc);

        Serial.println("Rebooting...");

        delay(esp_reboot_timeout);
        ESP.restart();
    }    
}