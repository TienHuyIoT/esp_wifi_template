#include "Esp.h"
#include "esp_reset.h"

#define RESET_DBG_PORT Serial
#define RESET_DBG_PRINTF(...) RESET_DBG_PORT.printf(__VA_ARGS__)

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

        RESET_DBG_PRINTF("Rebooting...");

        delay(esp_reboot_timeout);
        ESP.restart();
    }    
}