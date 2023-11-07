#ifndef	__RTC_TIME_TYPE_H
#define __RTC_TIME_TYPE_H

#define RTC_TIME_DEFAULT {1930, 1, 1, 3, 0, 0, 0}

/* =========================================================
 * RTC 
 * =========================================================*/
typedef struct rtc_time {
  int    year;   /* 2020 */
  int    mon;    /* 1..12 */
  int    mday;    /* 1.. 31 */
  int    wday;    /* 0..6 */
  int    hour;    /* 0..23 */
  int    min;    /* 0..59 */
  int    sec;    /* 0..59 */
} rtc_time_t;

#endif // __RTC_TIME_TYPE_H