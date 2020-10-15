#ifndef _TOOLS_H
#define _TOOLS_H
#ifdef __cplusplus
extern "C" {
#endif
#include "stdint.h"
#define LOWER_CASE	0
#define UPPER_CASE	1
char* BCDString(char *BCDStr,uint8_t *Buff, uint8_t Size, uint8_t UpperCase);
uint8_t StrncpyUntilChar(char *Dest,char *Scr, char Chr, uint16_t Leng);

#ifdef __cplusplus
}
#endif
#endif

