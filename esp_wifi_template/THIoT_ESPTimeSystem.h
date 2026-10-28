#ifndef __ESP_TIME_SYSTEM_H
#define __ESP_TIME_SYSTEM_H

#include <Arduino.h>
#include <functional>
#include <FS.h>
#include "THIoT_ESPTimeType.h"
#include "THIoT_PFTimeZone.h"
#include "THIoT_PFTicker.h"

typedef std::function<void(uint8_t, esp_rtc_time_t*)> espTimeHandle;

extern RTC_NOINIT_ATTR time_t noInitTimeSystem;

class RtcFileHandler
{
  using File = fs::File;
  using FS = fs::FS;
private:
  FS *_fs;

public:
  RtcFileHandler(FS &fs);
  ~RtcFileHandler();

  void save(esp_rtc_time_t *rtc);
  bool sync(esp_rtc_time_t *rtc);
  void remove(void);
};

class ESPTimeSystem
{
public:
  typedef enum : uint8_t
  {
    ESP_RTC_NON_UPDATE = 0,
    ESP_RTC_WEB_UPDATE,
    ESP_RTC_SNTP_UPDATE
  } level_update_t;

private:
  espTimeHandle  _espTimeCallback;
  ticker_function_handle_t _timeTicker;
  RtcFileHandler _rtcFile;
  level_update_t _rtcSource;

public:
  ESPTimeSystem(/* args */);
  ~ESPTimeSystem();

  void load();
  void saveToFileSystem();

  level_update_t getSourceUpdate() { return _rtcSource; }
  void onTimeLevelUpdate(espTimeHandle cb) { _espTimeCallback = cb;}
  void setSourceUpdate(level_update_t level);
  const char* printSourceUpdate(level_update_t level);

  void setTime(esp_rtc_time_t *rtc);

  void setDate(esp_rtc_time_t *rtc);

  void set(esp_rtc_time_t *rtc);

  void set(time_t t_now);

  void timeSystemUpdate() { noInitTimeSystem = now(); }

  /* Convert now to rtc */
  esp_rtc_time_t makeRtcFromNow(time_t t_now);

  /* Convert rtc to now */
  time_t makeNowFromRtc(esp_rtc_time_t *rtc);

  String toStringLog();
  
  String toStringConsole();

  String toString();

  String toString(esp_rtc_time_t *rtc);

  String toString(time_t t_now);

  /* Return time now number of rtc system */
  time_t now();

  /* Get rtc */
  esp_rtc_time_t get();
  esp_rtc_time_t get(time_t t_now);
  uint32_t hhMMssGet();
  String hhMMssToStr();

  uint32_t ddmmyyGet();
  String ddmmyyToStr();
  
  uint32_t yymmddGet();
  String yymmddToStr();
  String yyyymmddToStr();

  uint32_t hhmmssFormat(esp_rtc_time_t *rtc);

  uint32_t ddmmyyFormat(esp_rtc_time_t *rtc);
  uint32_t yymmddFormat(esp_rtc_time_t *rtc);

  uint32_t yyyymmddFormat(esp_rtc_time_t *rtc);

  /* hh:mm:ss */
  String hhmmssStr(uint32_t n);

  /* dd/mm/20yy */
  String ddmmyyStr(uint32_t n);

  String yyyymmddStr(uint32_t n);
  String yyyy_mm_ddStr(uint32_t n);

  time_t nowFromDdmmyyhhmmss(uint32_t ddmmyy, uint32_t hhmmss);
  time_t nowFromYyyymmddhhmmss(uint32_t yyyymmdd, uint32_t hhmmss);
  esp_rtc_time_t rtcFromDdmmyyhhmmss(uint32_t ddmmyy, uint32_t hhmmss);
  bool assertFromDdmmyyhhmmss(uint32_t ddmmyy, uint32_t hhmmss);
  bool assertFromRtc(esp_rtc_time_t &rtc);

  // "Tue" __DATE__ " " __TIME__ " GMT";
  bool GMTStringUpdate(const char *rtc_web, level_update_t level);

  void setTimeZone(long offset, int daylight);
  void setTzTime(const char* tz);
};

extern ESPTimeSystem ESPTime;

#endif // __ESP_TIME_SYSTEM_H
