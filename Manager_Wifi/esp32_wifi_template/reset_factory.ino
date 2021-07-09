#include "board.h"

#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
#include <IOInput.h>
#include "console_dbg.h"
#include "wifi_data_file.h"

#define FACTORY_PORT CONSOLE_PORT
#define RESET_FACTORY_PRINTF(...) CONSOLE_TAG_LOGI("[RESET FACTORY]", __VA_ARGS__)

extern IOInput input_factory_reset;
uint8_t factory_reset_handle(void)
{
    uint8_t result = 0;
    input_factory_reset.loop();
    if (input_factory_reset.GetStatus() == LOW)
    {
        uint32_t T = input_factory_reset.GetTime(LOW);
        if (T > 5000 && T < 10000)
        {
            input_factory_reset.set_low_time_ms(10000);

            wifi_info_refactor();
            RESET_FACTORY_PRINTF("OK");

            result = 1;
        }
    }

    return result;
}
#endif
