#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Tools.h"
/*
- In chuoi Buff
- BCDStr: chuoi sau khi chuyen
- Buff: la chuoi Hex
- Size: chieu dai mang hex can chuyen sang string, nen mang string co chieu dai la 2*len + 1
- Case: 1. chuyen dang chu Hoa, 0. chuyen dang chu thuong
#define LOWER_CASE	0
#define UPPER_CASE	1
- Vi du: hex[]={0x12,0xAB,0xD3}, len=3, hoathuong=1 -> String="12ABD3"
BCDString(String,hex,3,UPPER_CASE);
*/
char* BCDString(char *BCDStr,uint8_t *Buff, uint8_t Size, uint8_t Case){
    uint8_t i,j=0;
    for(i=0;i<Size;i++){
        if(Case==UPPER_CASE) sprintf(BCDStr+j,"%02X",Buff[i]);
        else sprintf(BCDStr+j,"%02x",Buff[i]);
        j+=2;
    }
    BCDStr[Size*2]='\0';
    return BCDStr;
}

/*Copy 1 chuoi ky tu den ky tu cuoi cung 'Chr'*/
uint8_t StrncpyUntilChar(char *Dest,char *Scr, char Chr, uint16_t Leng)
{
  uint16_t i;
  for(i=0;i<=Leng;i++){
    if(Scr[i]==Chr){
      strncpy(Dest,Scr,i);  
      Dest[i] = 0;
      return 1;
    }  
  } 
  return 0;   
}

