#ifndef __C_UTILITY_H
#define __C_UTILITY_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define LOWER_CASE 0
#define UPPER_CASE 1

#define CV_NUM2VOLUME 0
#define CV_NUM2CASH 1

/* u32 max is 10 numbers, add more 1 terminal character */
#define U32_TO_STRING_LENGTH_MAX 10

int vspfunc(size_t (*write_cb)(const uint8_t*, size_t), const char *format, ...);
void hex2str(char *str, uint8_t *hex, uint8_t len, uint8_t type);
char str2hex(const char *str, uint8_t *hex, uint8_t len);
uint8_t StrncpyUntilChar(char *Dest, char *Scr, char Chr, uint16_t Leng);
int find_char_rev(char *buff, char dat, int start_id);
char check_strncmp(const char *str, char dat, unsigned char *index, unsigned char len);
void convert_num2str_fpu(int32_t num, char *buff, uint8_t cmd);
uint8_t crc_xor(const uint8_t *buff, uint32_t lenght);
uint8_t u_itoa(uint32_t value, char* buff);
uint8_t long_strtok(char* str, const char* delimiters, long* array, uint8_t length);
uint8_t p_strtok(char* str, const char* delimiters, char* pArray[], uint8_t size);

#define CV_NUM2VOLUME 0
#define CV_NUM2CASH 1

#ifdef __cplusplus
}
#endif
#endif // __C_UTILITY_H
