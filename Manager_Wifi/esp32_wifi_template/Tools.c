#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Tools.h"
/*
- In chuoi Buff
- str: chuoi sau khi chuyen
- hex: la chuoi Hex
- len: chieu dai mang hex can chuyen sang string, nen mang string co chieu dai la 2*len + 1
- type: 1. chuyen dang chu Hoa, 0. chuyen dang chu thuong
#define LOWER_CASE	0
#define UPPER_CASE	1
- Vi du: hex[]={0x12,0xAB,0xD3}, len=3, hoathuong=1 -> String="12ABD3"
hex2str(String,hex,3,UPPER_CASE);
*/
void hex2str(char *str, uint8_t *hex, uint8_t len, uint8_t type)
{
  uint8_t i, j = 0;
  for (i = 0; i < len; i++)
  {
    if (type == UPPER_CASE)
      sprintf(str + j, "%02X", hex[i]);
    else
      sprintf(str + j, "%02x", hex[i]);
    j += 2;
  }
  str[len * 2] = '\0';
}

/*
- Chuyen string thanh Hex
- Len: Chieu dai chuoi String muon chuyen thanh Hex
- Vi du: String="12ABD3", len=6 -> hex[]={0x12,0xAB,0xD3}
Ghi chu: String phai la UPPER_CASE
*/
char str2hex(const char *str, uint8_t *hex, uint8_t len)
{
  uint8_t i, j;
  uint8_t high, low;
  j = 0;
  i = 0;
  while (1)
  {
    if (str[j] >= '0' && str[j] <= '9')
    {
      high = str[j] - '0';
    }
    else if (str[j] >= 'A' && str[j] <= 'F')
    {
      high = str[j] - 'A' + 10;
    }
    else
      return 0;
    hex[i] = (high << 4);
    if (++j >= len)
      break;

    if (str[j] >= '0' && str[j] <= '9')
    {
      low = str[j] - '0';
    }
    else if (str[j] >= 'A' && str[j] <= 'F')
    {
      low = str[j] - 'A' + 10;
    }
    else
      return 0;
    hex[i] |= low;
    if (++j >= len)
      break;
    i++;
  }
  return 1;
}

/*Copy 1 chuoi ky tu den ky tu cuoi cung 'Chr'*/
uint8_t StrncpyUntilChar(char *Dest, char *Scr, char Chr, uint16_t Leng)
{
  uint16_t i;
  for (i = 0; i <= Leng && Scr[i] != 0; i++)
  {
    if (Scr[i] == Chr)
    {
      strncpy(Dest, Scr, i);
      Dest[i] = 0;
      return 1;
    }
  }
  return 0;
}

char check_strncmp(char *str, char dat, uint8_t *index, uint8_t len)
{
  if (str[*index] == dat)
  {
    (*index)++;
    if ((*index) == len)
    {
      *index = 0;
      return 1;
    }
  }
  else
  {
    *index = 0;
    if (str[*index] == dat)
      (*index)++;
  }
  return 0;
}

void convert_num2str_fpu(uint32_t num, char *buff, uint8_t cmd)
{
  if (cmd == CV_NUM2VOLUME)
  {
    sprintf(buff, "%u,%03u", num / 1000, num % 1000);
  }
  else
  {
    if (num < 1000)
    {
      sprintf(buff, "%u", num);
    }
    else if (num < 1000000)
    {
      sprintf(buff, "%u.%03u", num / 1000, num % 1000);
    }
    else if (num < 1000000000)
    {
      sprintf(buff, "%u.%03u.%03u", num / 1000000, (num % 1000000) / 1000, num % 1000);
    }
    else
    {
      sprintf(buff, "%u.%03u.%03u.%03u", num / 1000000000, (num % 1000000000) / 1000000, (num % 1000000) / 1000, num % 1000);
    }
  }
}

uint8_t crc_xor(const uint8_t *buff, uint32_t lenght)
{
  uint8_t xor = 0;
  for (uint32_t i = 0; i < lenght; ++i)
  {
    xor ^= buff[i];
  }
  return xor;
}
