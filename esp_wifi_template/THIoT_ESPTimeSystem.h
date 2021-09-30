#ifndef __ESP_TIME_SYSTEM_H
#define __ESP_TIME_SYSTEM_H

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

class RtcFileHandler
{
private:
  fs::FS *_fs;

public:
  RtcFileHandler(fs::FS &fs);
  ~RtcFileHandler();

  void save(rtc_time_t *rtc);
  bool sync(rtc_time_t *rtc);
  void remove(void);
};

class ESPTimeSystem
{
public:
  typedef enum : uint8_t
  {
    RTC_NON_UPDATE = 0,
    RTC_WEB_UPDATE,
    RTC_SNTP_UPDATE
  } level_update_t;

private:
  RtcFileHandler *_rtcFile;
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
  ESPTimeSystem(/* args */);
  ~ESPTimeSystem();

  void load();

  void saveToFileSystem();

  level_update_t getSourceUpdate() { return _rtcSource; }

  void setSourceUpdate(level_update_t level);

  void setTime(rtc_time_t *rtc);

  void setDate(rtc_time_t *rtc);

  void set(rtc_time_t *rtc);

  void set(time_t t_now);

  /* Convert now to rtc */
  rtc_time_t makeRtcFromNow(time_t t_now);

  /* Convert rtc to now */
  time_t makeNowFromRtc(rtc_time_t *rtc);

  String toStringLog();

  String toString();

  String toString(rtc_time_t *rtc);

  String toString(time_t t_now);

  /* Return time now number of rtc system */
  time_t now();

  /* Get rtc */
  rtc_time_t get();

  uint32_t hhmmssFormat(rtc_time_t *rtc);

  uint32_t ddmmyyFormat(rtc_time_t *rtc);

  // "Tue" __DATE__ " " __TIME__ " GMT";
  bool GMTStringUpdate(const char *rtc_web, level_update_t level);

  void setTimeZone(long offset, int daylight);
};

extern ESPTimeSystem ESPTime;

#endif // __ESP_TIME_SYSTEM_H
