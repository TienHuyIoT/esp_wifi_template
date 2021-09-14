#include <Esp.h>
#include "app_config.h"
#include "hth_esp_soft_reset.h"
#include "hth_esp_sys_rtc.h"
#include "hth_console_dbg.h"

#define RESET_DBG_PORT CONSOLE_PORT
#define RESET_DBG_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define RESET_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[RESET]", __VA_ARGS__)

void hth_esp_soft_reset::enable(uint32_t timeout)
{
    _reset.once_ms(timeout, [](){
        HTH_sysTime.saveToFS();
        RESET_TAG_CONSOLE("\tRebooting...");
        delay(10);
        ESP.restart();
    });
}

hth_esp_soft_reset HTH_softReset;
