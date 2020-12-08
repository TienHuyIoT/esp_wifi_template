#include "esp_reset.h"

// need non block function
void esp_reset_enable()
{
    // save rtc current into fs before restart
    rtc_time_t rtc;
    rtc_get(&rtc);
    rtc_info_write(&rtc);

    Serial.println("Rebooting...");

    delay(500);
    ESP.restart();
}

void esp_reboot(void)
{
    if (shouldReboot)
    {
        esp_reset_enable();
    }    
}