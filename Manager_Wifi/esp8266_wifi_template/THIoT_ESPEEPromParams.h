#ifndef	__ESP_EEPROM_PARAMS_H
#define __ESP_EEPROM_PARAMS_H

#include <Arduino.h>

#define EEPROM_BYTE_SIZE         512
#define EEPROM_DEVICE_INACTIVE   0
#define EEPROM_DEVICE_ACTIVE     1

class ESPEEPromParams
{
    typedef enum : uint8_t {
        ACTIVE_READ_CM = 0,
        ACTIVE_ENABLE_CM,
        ACTIVE_DISABLE_CM
    } active_cmd_t;

    typedef enum : uint8_t {
        DEVICE_INACTIVE = 0,
        DEVICE_ACTIVATED
    } device_active_t;

    typedef struct {
        union {
            uint32_t block0;
            struct {
                uint8_t autoCfg;
                uint8_t activeStatus;
            };
        };
        union {
            uint64_t chipID;
            uint32_t block1_2[2];
            struct {
                byte mac[6];
            };
        };
    } eeprom_data_t;

private:
    eeprom_data_t _eepData;
    void activePerform(active_cmd_t Cmd);

public:
    ESPEEPromParams(/* args */);
    ~ESPEEPromParams();

    uint8_t isDeviceActivated();
    void deviceActive();
    void deviceInactive();
    void load();
};

extern ESPEEPromParams EEPParams;

#endif /* !__ESP_EEPROM_PARAMS_H */
