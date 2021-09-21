#include <Esp.h>
#include "hth_esp_config.h"
#include "THIoT_ESPSoftReset.h"
#include "hth_esp_sys_rtc.h"
#include "hth_serial_trace.h"

#define RESET_DBG_PORT CONSOLE_PORT
#define RESET_DBG_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define RESET_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[RESET]", __VA_ARGS__)

void ESPSoftReset::enable(uint32_t timeout)
{
    _reset.once_ms(timeout, [](){
        ESPTime.saveToFileSystem();
        RESET_TAG_CONSOLE("\tRebooting...");
        CONSOLE_FLUSH();
        ESP.restart();
    });
}

ESPSoftReset HTH_softReset;
