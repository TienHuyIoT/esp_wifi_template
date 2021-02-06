#define FACTORY_PORT Serial
#define RESET_FACTORY_PRINTF(fmt, ...) FACTORY_PORT.printf("\r\n>RESET FACTORY< " fmt, ##__VA_ARGS__)

#include <IOInput.h>
#include "board.h"

#if (defined FACTORY_INPUT_PIN) && (FACTORY_INPUT_PIN != -1)
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
