#ifndef	_EEPROM_DATA_
#define _EEPROM_DATA_

#define EEPROM_SIZE_MAX           512

#define Df_ReadEep                0
#define Df_UpdateEep              1

#define Df_Mac_InActivate         0
#define Df_Mac_Activated          1

#define Df_ConfigActiveRead       0
#define Df_ConfigActiveUpdate     1
#define Df_ConfigInActiveUpdate   2

typedef struct {
    union {
        uint32_t Block0;
        struct {
            uint8_t AutoConfig;
            uint8_t Activated;
        };
    };
    union {
        uint64_t chipid;
        uint32_t Block1_2[2];
        struct {
            byte mac[6];
        };
    };
} eeprom_data_t;

uint8_t ActivateByMac = Df_Mac_InActivate;
eeprom_data_t eep_data_struct;

#endif