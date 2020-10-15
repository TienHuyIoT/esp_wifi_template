#ifndef _TOOLS_H
#define _TOOLS_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "stdint.h"
#define LOWER_CASE 0
#define UPPER_CASE 1
    void hex2str(char *str, uint8_t *hex, uint8_t len, uint8_t type);
    char str2hex(const char *str, uint8_t *hex, uint8_t len);
    uint8_t StrncpyUntilChar(char *Dest, char *Scr, char Chr, uint16_t Leng);
    char check_strncmp(char *str, char dat, unsigned char *index, unsigned char len);
    void convert_num2str_fpu(uint32_t num, char *buff, uint8_t cmd);
    uint8_t crc_xor(const uint8_t *buff, uint32_t lenght);

#define CV_NUM2VOLUME 0
#define CV_NUM2CASH 1

#ifdef __cplusplus
}
#endif
#endif
