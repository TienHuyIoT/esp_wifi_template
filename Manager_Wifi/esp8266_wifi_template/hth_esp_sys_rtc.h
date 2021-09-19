#ifndef __RTC_H
#define __RTC_H

#include <Arduino.h>
#include <FS.h>

typedef struct rtc_time
{
  int year; /** yyyy */
  int mon;  /** 1..12 */
  int mday; /** 1.. 31 */
  int wday; /** sun(0) ... sat(6)*/
  int hour; /** 0..23 */
  int min;  /** 0..59 */
  int sec;  /** 0..59 */
} rtc_time_t;

class rtc_data_file
{
private:
  fs::FS *_fs;

public:
  rtc_data_file(fs::FS &fs);
  ~rtc_data_file();

  void save(rtc_time_t *rtc);
  bool sync(rtc_time_t *rtc);
  void remove(void);
};

class hth_esp_sys_rtc
{
public:
  typedef enum : uint8_t
  {
    RTC_NON_UPDATE = 0,
    RTC_WEB_UPDATE,
    RTC_SNTP_UPDATE
  } level_update_t;

private:
  rtc_data_file *_rtcFile;
  level_update_t _rtcSource;

  const char *printSourceUpdate(level_update_t level)
  {
    const char *str;
    switch (level)
    {
    case RTC_NON_UPDATE:
      str = (const char *)"RTC_NON_UPDATE";
      break;

    case RTC_WEB_UPDATE:
      str = (const char *)"RTC_WEB_UPDATE";
      break;

    case RTC_SNTP_UPDATE:
      str = (const char *)"RTC_SNTP_UPDATE";
      break;

    default:
      str = (const char *)"RTC_UNKNOW_UPDATE";
      break;
    }

    return (const char *)str;
  }

public:
  hth_esp_sys_rtc(/* args */);
  ~hth_esp_sys_rtc();

  void begin();

  void saveToFS();

  level_update_t getSourceUpdate() { return _rtcSource; }

  void setSourceUpdate(level_update_t level);

  void setTime(rtc_time_t *rtc);

  void setDate(rtc_time_t *rtc);

  void setTimeDate(rtc_time_t *rtc);

  /* Convert now to rtc */
  rtc_time_t now2Time(time_t t_now);

  /* Convert rtc to now */
  time_t time2Now(rtc_time_t *rtc);

  char *printTimeFromNow(time_t t_now);

  /* Return time now number of rtc system */
  time_t nowCurrent();

  /* Get rtc */
  bool getTimeDate(rtc_time_t *rtc);

  uint32_t hhmmssFormat(rtc_time_t *rtc);

  uint32_t ddmmyyFormat(rtc_time_t *rtc);

  // "Tue" __DATE__ " " __TIME__ " GMT";
  bool GMTStringUpdate(const char *rtc_web, level_update_t level);
};

extern hth_esp_sys_rtc HTH_sysTime;

#endif
