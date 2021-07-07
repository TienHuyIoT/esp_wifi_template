#ifndef	__RTC_H
#define __RTC_H

#include "Arduino.h"
#include "rtc_data_file.h"

void rtc_setup(void);

uint8_t rtc_level_update_get(void);

void rtc_level_update_set(uint8_t level);

void rtc_set_time(rtc_time_t *rtc);

void rtc_set_date(rtc_time_t *rtc);

void rtc_set(rtc_time_t *rtc);

/* Convert now to rtc */
void rtc_localtime(rtc_time_t* rtc, uint32_t t_now);

/* Convert rtc to t_now */
uint32_t rtc_mktime(rtc_time_t* rtc);

void rtc_printtime_tnow(uint32_t t_now);

/* Return time now number of rtc system */
uint32_t rtc_get_t_now(void);

/* Get rtc */
uint8_t rtc_get(rtc_time_t *rtc);

uint32_t time_hhmmss_format(rtc_time_t *rtc) ;

uint32_t date_ddmmyy_format(rtc_time_t *rtc) ;

#endif