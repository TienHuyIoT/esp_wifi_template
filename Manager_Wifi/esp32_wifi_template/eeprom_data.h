#ifndef	_EEPROM_DATA_
#define _EEPROM_DATA_

#define EEPROM_SIZE_MAX 512

#define Df_ReadEep     0
#define Df_UpdateEep   1

#define Df_MacUnActivate     0
#define Df_MacActivated      1

#define Df_ConfigActiveRead       0
#define Df_ConfigActiveUpdate     1
#define Df_ConfigUnActiveUpdate   2

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

#endif