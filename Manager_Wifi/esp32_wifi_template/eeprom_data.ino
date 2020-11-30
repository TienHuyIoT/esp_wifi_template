#include "eeprom_data.h"

#define EEPROM_DATA_PORT Serial
#define EEPROM_DATA_PRINTF(f_, ...) EEPROM_DATA_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

static uint8_t ActivateByMac = Df_MacUnActivate;
static eeprom_data_t eep_data_struct;

uint8_t device_is_active()
{
    return (Df_MacActivated == eep_data_struct.Activated);
}

void device_active()
{
    active_configure(Df_MacActivated);
}

void device_inactive()
{
    active_configure(Df_MacUnActivate);
}

void eeprom_setup()
{
    uint64_t chipid;
    chipid = ESP.getEfuseMac();
    EEPROM_DATA_PORT.printf("\r\nESP32 Chip ID = %04X", (uint16_t)(chipid >> 32));//print High 2 bytes
    EEPROM_DATA_PORT.printf("%08X\r\n", (uint32_t)chipid);//print Low 4bytes.

    EEPROM.begin(EEPROM_SIZE_MAX);
    EEPROM.get(0, eep_data_struct);
    EEPROM_DATA_PRINTF("\r\nEeprom Info:");
    EEPROM_DATA_PRINTF("\r\n- AutoConfig: %u", eep_data_struct.AutoConfig);
    EEPROM_DATA_PRINTF("\r\n- Activated: %u\r\n", eep_data_struct.Activated);
    EEPROM_DATA_PRINTF("\r\n- Chipid: %04X%08X", eep_data_struct.Block1_2[1], eep_data_struct.Block1_2[0]);
    if (128 != eep_data_struct.AutoConfig)
    {
        eep_data_struct.AutoConfig = 128;
        eep_data_struct.Activated = 0;
        eep_data_struct.chipid = 0;
        EEPROM.put(0, eep_data_struct);
    }
    EEPROM.end();

    //Nếu địa chỉ Mac không được activated
    if(eep_data_struct.chipid != chipid)
    {
        eep_data_struct.Activated = Df_MacUnActivate;
        EEPROM_DATA_PRINTF("\r\nDevice inactive\r\n");
    }
    else
    {
        eep_data_struct.Activated = Df_MacActivated;
        EEPROM_DATA_PRINTF("\r\nDevice activated\r\n");
    }
}

void active_configure(uint8_t Cmd)
{
    uint64_t chipid;
    chipid = ESP.getEfuseMac();
    EEPROM_DATA_PORT.printf("\r\nESP32 Chip ID = %04X", (uint16_t)(chipid >> 32));//print High 2 bytes
    EEPROM_DATA_PORT.printf("%08X\r\n", (uint32_t)chipid);//print Low 4bytes.
    EEPROM_DATA_PRINTF("\r\nActive configure: %u", Cmd);

    if (Df_ConfigActiveUpdate == Cmd) {
        eep_data_struct.chipid = chipid;
        eep_data_struct.Activated = Df_MacActivated;
    }

    if (Df_ConfigUnActiveUpdate == Cmd) {
        eep_data_struct.chipid = 0;
        eep_data_struct.Activated = Df_MacUnActivate;
    }

    EEPROM.begin(EEPROM_SIZE_MAX);
    EEPROM.put(0, eep_data_struct);
    EEPROM.end();
}