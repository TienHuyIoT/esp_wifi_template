#include "Esp.h"
#include "esp_reset.h"
#include "rtc_data_file.h"
#include "flatform_rtc.h"
#include "console_dbg.h"

#define RESET_DBG_PORT CONSOLE_PORT
#define RESET_DBG_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)

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

        RESET_DBG_PRINTF("\r\n\tRebooting...");

        delay(esp_reboot_timeout);
        ESP.restart();
    }    
}