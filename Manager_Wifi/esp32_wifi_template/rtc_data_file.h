#ifndef	_RTC_DATA_FILE_
#define _RTC_DATA_FILE_

#define RTC_FILE_PATH "/rtc_info_file.txt"

#define RTC_TIME_DEFAULT {1930, 1, 1, 3, 0, 0, 0}

typedef enum {
    RTC_FILE_READ = 0,
    RTC_FILE_WRITE
} rtc_file_cmd_t;

typedef enum {
    RTC_NON_UPATE = 0,
    RTC_WEB_UPATE,
    RTC_SNTP_UPDATE
} rtc_level_update_t;

/* =========================================================
 * RTC 
 * =========================================================*/
typedef struct rtc_time{
  int    year;   /* 2020 */
  int    mon;    /* 1..12 */
  int    mday;    /* 1.. 31 */
  int    wday;    /* 0..6 */
  int    hour;    /* 0..23 */
  int    min;    /* 0..59 */
  int    sec;    /* 0..59 */
} rtc_time_t;

uint8_t rtc_level_update = RTC_NON_UPATE;

void rtc_level_update_set(uint8_t level);
uint8_t rtc_level_update_get(void);

#endif