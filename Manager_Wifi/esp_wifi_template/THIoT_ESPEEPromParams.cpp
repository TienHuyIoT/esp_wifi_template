#include <EEPROM.h>
#include "THIoT_ESPEEPromParams.h"
#include "THIoT_SerialTrace.h"

#define EEPROM_DATA_PORT SERIAL_PORT
#define EEPROM_DATA_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#define EEPROM_DATA_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[EEPROM]", __VA_ARGS__)

ESPEEPromParams::ESPEEPromParams(/* args */)
{
    memset(&_eepData, 0, sizeof(_eepData));
}

ESPEEPromParams::~ESPEEPromParams()
{
}

uint8_t ESPEEPromParams::isDeviceActivated()
{
    return ((uint8_t)DEVICE_ACTIVATED == _eepData.activeStatus);
}

void ESPEEPromParams::deviceActive()
{
    activePerform(ACTIVE_ENABLE_CM);
}

void ESPEEPromParams::deviceInactive()
{
    activePerform(ACTIVE_DISABLE_CM);
}

void ESPEEPromParams::load()
{
    uint64_t chipID;
#ifdef ESP32
    chipID = ESP.getEfuseMac();
#elif defined(ESP8266)
    chipID = ESP.getChipId();
#endif
    EEPROM_DATA_TAG_CONSOLE("Chip ID = %04X%08X", (uint16_t)(chipID >> 32), (uint32_t)chipID);

    EEPROM.begin(EEPROM_BYTE_SIZE);
    EEPROM.get(0, _eepData);
    EEPROM_DATA_TAG_CONSOLE("Eeprom Information:");
    EEPROM_DATA_TAG_CONSOLE("- autoCfg: %u", _eepData.autoCfg);
    EEPROM_DATA_TAG_CONSOLE("- activeStatus: %u", _eepData.activeStatus);
    EEPROM_DATA_TAG_CONSOLE("- Chipid: %04X%08X", _eepData.block1_2[1], _eepData.block1_2[0]);

    constexpr uint8_t AUTO_CONFIG_IDENTIFICATION_VALUE = 128;
    if (AUTO_CONFIG_IDENTIFICATION_VALUE != _eepData.autoCfg)
    {
        _eepData.autoCfg = AUTO_CONFIG_IDENTIFICATION_VALUE;
        _eepData.activeStatus = 0;
        _eepData.chipID = 0;
        EEPROM.put(0, _eepData);
    }
    EEPROM.end();

    if (_eepData.chipID != chipID)
    {
        _eepData.activeStatus = DEVICE_INACTIVE;
        EEPROM_DATA_TAG_CONSOLE("Device inactive");
    }
    else
    {
        _eepData.activeStatus = DEVICE_ACTIVATED;
        EEPROM_DATA_TAG_CONSOLE("Device activated");
    }
}

void ESPEEPromParams::activePerform(active_cmd_t Cmd)
{
    uint64_t chipID;
#ifdef ESP32
    chipID = ESP.getEfuseMac();
#elif defined(ESP8266)
    chipID = ESP.getChipId();
#endif
    EEPROM_DATA_TAG_CONSOLE("Chip ID = %04X", (uint16_t)(chipID >> 32)); //print 2 bytes High
    EEPROM_DATA_CONSOLE("%08X\r\n", (uint32_t)chipID);                   //print 4bytes Low.

    if (ACTIVE_ENABLE_CM == Cmd)
    {
        _eepData.chipID = chipID;
        _eepData.activeStatus = EEPROM_DEVICE_ACTIVE;
    }

    if (ACTIVE_DISABLE_CM == Cmd)
    {
        _eepData.chipID = 0;
        _eepData.activeStatus = EEPROM_DEVICE_INACTIVE;
    }

    EEPROM.begin(EEPROM_BYTE_SIZE);
    EEPROM.put(0, _eepData);
    EEPROM.end();
}

ESPEEPromParams EEPParams;
