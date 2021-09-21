#include <Esp.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_SerialTrace.h"

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

ESPSoftReset SOFTReset;
