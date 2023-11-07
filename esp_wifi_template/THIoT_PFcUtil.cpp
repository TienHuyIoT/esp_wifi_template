#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <Arduino.h>
#include "THIoT_PFCommonUtil.h"
#include "THIoT_PFcUtil.h"

#define V_BUFFER    256
int vspfunc(size_t (*write_cb)(uint8_t*, size_t), const char *format, ...)
{
  va_list args;
  char* v_buff = (char*)malloc(V_BUFFER);
  size_t length;

  va_start(args, format);
  length = vsnprintf(v_buff, V_BUFFER, format, args);
  va_end(args);

  if (length > 0)
  {
    write_cb((uint8_t*)v_buff, length);
  }
  free(v_buff);
  return length;
}

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

int find_char_rev(char *buff, char dat, int start_id)
{
  int i;
  for(i = start_id - 1; i >= 0; --i)
  {
    if (buff[i] == dat)
    {
      break;
    }
  }
  return i;
}

char check_strncmp(const char *str, char dat, uint8_t *index, uint8_t len)
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

void convert_num2str_fpu(int32_t num, char *buff, uint8_t cmd)
{
  int32_t num_abs;
  num_abs = abs(num);

  if (cmd == CV_NUM2VOLUME)
  {
    sprintf(buff, "%ld,%03lu", num / 1000, num_abs % 1000);
  }
  else
  {
    if (num_abs < 1000)
    {
      sprintf(buff, "%ld", num);
    }
    else if (num_abs < 1000000)
    {
      sprintf(buff, "%ld.%03lu", num / 1000, num_abs % 1000);
    }
    else if (num_abs < 1000000000)
    {
      sprintf(buff, "%ld.%03lu.%03lu", num / 1000000, (num_abs % 1000000) / 1000, num_abs % 1000);
    }
    else
    {
      sprintf(buff, "%ld.%03lu.%03lu.%03lu", num / 1000000000, (num_abs % 1000000000) / 1000000, (num_abs % 1000000) / 1000, num_abs % 1000);
    }
  }
}

uint8_t crc_xor(const uint8_t *buff, uint32_t length)
{
  uint8_t crc = 0;
  for (uint32_t i = 0; i < length; ++i)
  {
    crc ^= buff[i];
  }
  return crc;
}

/* Brief: swap ddmmyy --> 20yymmdd
 * 
 */
uint32_t swap_date(uint32_t date_in)
{
  uint32_t dd,mm,yy, date_out;
  yy = date_in % 100;
  mm = (date_in / 100) % 100;
  dd = date_in / 10000;
  date_out = 20000000 + (yy * 10000) + (mm * 100) + dd;
  return date_out;
}

int str_cmp_find_index(const char *const *literal, const char *strIn)
{
    int index = -1;
    for (int i = 0; strlen(literal[i]) > 0; ++i)
    {
        if (strcmp(strIn, literal[i]) == 0)
        {
            index = i;
        }
    }

    return index;
}
