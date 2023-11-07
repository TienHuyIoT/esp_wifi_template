#include <Arduino.h>
#include <time.h> // time() ctime()
#include <sys/time.h>
#ifdef ESP8266
#include <sys/time.h> // struct timeval
#endif
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "THIoT_APPConfig.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPSysParams.h"
#include "THIoT_ESPTimeSystem.h" 
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"


#define RTC_CONSOLE_PORT            SERIAL_PORT
#define RTC_CONSOLE(...)            SERIAL_LOGI(__VA_ARGS__)
#define RTC_TAG_CONSOLE(...)        SERIAL_TAG_LOGI("[RTC]", __VA_ARGS__)

#define RTC_DATA_CONSOLE_PORT       SERIAL_PORT
#define RTC_DATA_CONSOLE(...)       SERIAL_LOGI(__VA_ARGS__)
#define RTC_DATA_TAG_CONSOLE(...)   SERIAL_TAG_LOGI("[RTC DATA]", __VA_ARGS__)
#define RTC_FUNCTION_TAG_CONSOLE(...) //SERIAL_FUNCTION_TAG_LOGI("[RTC]", __VA_ARGS__)

#define RTC_DATA_TAG_LOG(...)       FS_TAG_LOGI("[RTC] ", __VA_ARGS__)

#define RTC_YEAR_BEGIN 1900
#define ESP_RTC_FILE_PATH ((const char*)"/rtc_info_file.txt")

const char *const mon_list[] PROGMEM = {
            "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static void time_level_update_log() {
    ESPTimeSystem::level_update_t level = ESPTime.getSourceUpdate();
    RTC_DATA_TAG_LOG("Access Update the level: %s", ESPTime.printSourceUpdate(level));
}

#ifdef ESP8266
static bool getLocalTime(struct tm *info, uint32_t ms) {
    uint32_t start = millis();
    time_t now;
    while ((millis() - start) <= ms) {
        time(&now);
        localtime_r(&now, info);
        if (info->tm_year >= (2021 - RTC_YEAR_BEGIN)) {
            return true;
        }
        delay(10);
    }
    return false;
}
#endif

ESPTimeSystem::ESPTimeSystem(/* args */)
: _rtcFile(NAND_FS_SYSTEM) {
    _rtcSource = level_update_t::ESP_RTC_NON_UPDATE;
}

ESPTimeSystem::~ESPTimeSystem() {}

void ESPTimeSystem::load(void) {
    struct tm tmStruct;
    esp_rtc_time_t rtc;

    if (!getLocalTime(&tmStruct, 5)) {
        if (_rtcFile.sync(&rtc)) {
            _rtcFile.remove();
            // rtc.year = 2020;
            // rtc.mon = 8;
            // rtc.mday = 2;
            // rtc.hour = 22;
            // rtc.min = 51;
            // rtc.sec = 0;
            // rtc.wday = 0; // sunday(0), sun(0) ... sat(6)
            RTC_TAG_CONSOLE("Update RTC from file system");
            set(&rtc);
        }
        else {
            RTC_TAG_CONSOLE("Init build time system");
            const char *sysTime = "Tue " __DATE__ " " __TIME__ " GMT";
            GMTStringUpdate(sysTime, level_update_t::ESP_RTC_NON_UPDATE);
        }
    }
    else {
#if (TIME_ZONE_TYPE_CFG == 1)
        setTimeZone(-ESPConfig.gmtOffsetSNTP() * 3600, ESPConfig.daylightOffsetSNTP());
#else
        setTzTime(ESPConfig.TzTime().c_str());
#endif
    }

    RTC_TAG_CONSOLE("Time: %s", toString().c_str());
}

void ESPTimeSystem::saveToFileSystem() {
    esp_rtc_time_t rtc;
    RTC_TAG_CONSOLE("Save Time to File system");
    rtc = get();
    _rtcFile.save(&rtc);
}

void ESPTimeSystem::setSourceUpdate(level_update_t level) {
    if (level >= _rtcSource) {
        esp_rtc_time_t rtc;
        RTC_TAG_CONSOLE("Access Update the level: %s", printSourceUpdate(level));
        _rtcSource = level;
        rtc = get();
        
        ticker_once(&_logTicker, 1, [](void *arg){
            time_level_update_log();
        }, nullptr);
        
        if (_espTimeCallback) {
            _espTimeCallback(level, &rtc);
        }
    }
    else {
        RTC_TAG_CONSOLE("Deny Update the level: %s", printSourceUpdate(level));
    }
}

void ESPTimeSystem::setTime(esp_rtc_time_t *rtc) {
    esp_rtc_time_t g_rtc;
    g_rtc = get();
    g_rtc.hour = rtc->hour;
    g_rtc.min = rtc->min;
    g_rtc.sec = rtc->sec;
    set(&g_rtc);
}

void ESPTimeSystem::setDate(esp_rtc_time_t *rtc) {
    esp_rtc_time_t g_rtc;
    g_rtc = get();
    g_rtc.year = rtc->year;
    g_rtc.mon = rtc->mon;
    g_rtc.mday = rtc->mday;
    set(&g_rtc);
}

void ESPTimeSystem::set(esp_rtc_time_t *rtc) {
    struct timeval t_val;
    time_t t_now = 0;

    RTC_FUNCTION_TAG_CONSOLE("RTC IN");
    t_now = makeNowFromRtc(rtc);
    t_val = {.tv_sec = t_now};
    settimeofday(&t_val, NULL);
    RTC_FUNCTION_TAG_CONSOLE("RTC OUT");
}

void ESPTimeSystem::set(time_t t_now) {
    struct timeval t_val;
    RTC_FUNCTION_TAG_CONSOLE("NOW IN");
    t_val = {.tv_sec = t_now};
    settimeofday(&t_val, NULL);
    RTC_FUNCTION_TAG_CONSOLE("NOW OUT");
}

/* Convert t_now to rtc */
esp_rtc_time_t ESPTimeSystem::makeRtcFromNow(time_t t_now) {
    struct tm desired_tm;
    esp_rtc_time_t rtc;
    localtime_r((time_t *)&t_now, &desired_tm);
    rtc.mon = desired_tm.tm_mon + 1;
    rtc.mday = desired_tm.tm_mday;
    rtc.year = desired_tm.tm_year + RTC_YEAR_BEGIN;
    rtc.hour = desired_tm.tm_hour;
    rtc.min = desired_tm.tm_min;
    rtc.sec = desired_tm.tm_sec;
    rtc.wday = desired_tm.tm_wday;

    RTC_FUNCTION_TAG_CONSOLE("IN %lu", t_now);
    RTC_FUNCTION_TAG_CONSOLE("OUT %u %u-%u-%u %u:%u:%u",
                   rtc.wday, rtc.year, rtc.mon, rtc.mday, 
                   rtc.hour, rtc.min, rtc.sec);
    return rtc;
}

/* Convert rtc to now */
time_t ESPTimeSystem::makeNowFromRtc(esp_rtc_time_t *rtc) {
    struct tm desired_tm;
    time_t t_now = 0;
    memset(&desired_tm, 0, sizeof(struct tm));
    desired_tm.tm_mon = rtc->mon - 1;
    desired_tm.tm_mday = rtc->mday;
    desired_tm.tm_year = rtc->year - RTC_YEAR_BEGIN;
    desired_tm.tm_hour = rtc->hour;
    desired_tm.tm_min = rtc->min;
    desired_tm.tm_sec = rtc->sec;
    desired_tm.tm_wday = rtc->wday;
    t_now = mktime(&desired_tm);

    RTC_FUNCTION_TAG_CONSOLE("IN %u %u-%u-%u %u:%u:%u",
                   rtc->wday, rtc->year, rtc->mon, rtc->mday, 
                   rtc->hour, rtc->min, rtc->sec);
    RTC_FUNCTION_TAG_CONSOLE("OUT %lu", t_now);
    return t_now;
}

String ESPTimeSystem::toStringLog() {
    constexpr uint8_t BUFFER_TIME_LENGTH_MAX = 80;
    char buf[BUFFER_TIME_LENGTH_MAX];
    time_t now = time(nullptr);
    const tm* tm = localtime(&now);
    strftime(buf, sizeof(buf), "%Y/%m/%d, %H:%M:%S, ", tm);
    return String(buf);
}

String ESPTimeSystem::toString() {
    constexpr uint8_t BUFFER_TIME_LENGTH_MAX = 80;
    char buf[BUFFER_TIME_LENGTH_MAX];
    time_t now = time(nullptr);
    const tm* tm = localtime(&now);
    strftime(buf, sizeof(buf), "%A %Y-%m-%d %H:%M:%S %Z", tm);
    return String(buf);
}

String ESPTimeSystem::toString(esp_rtc_time_t *rtc) {
    constexpr uint8_t BUFFER_TIME_LENGTH_MAX = 80;
    char buf[BUFFER_TIME_LENGTH_MAX];

    RTC_TAG_CONSOLE("%u %u-%u-%u %u:%u:%u",
                   rtc->wday, rtc->year, rtc->mon, rtc->mday, 
                   rtc->hour, rtc->min, rtc->sec);

    time_t now = makeNowFromRtc(rtc);
    const tm* tm = localtime(&now);
    strftime(buf, sizeof(buf), "%A %Y-%m-%d %H:%M:%S %Z", tm);
    return String(buf);
}

String ESPTimeSystem::toString(time_t t_now) {
    // ctime(&t_now);
    constexpr uint8_t BUFFER_TIME_LENGTH_MAX = 80;
    char buf[BUFFER_TIME_LENGTH_MAX];
    time_t now = t_now;
    const tm* tm = localtime(&now);
    strftime(buf, sizeof(buf), "%A %Y-%m-%d %H:%M:%S %Z", tm);
    return String(buf);
}

time_t ESPTimeSystem::now() {
    time_t now = time(nullptr);
    // RTC_TAG_CONSOLE("now = %lu", now);
    return now;
}

esp_rtc_time_t ESPTimeSystem::get() {
    esp_rtc_time_t rtc = RTC_TIME_DEFAULT;
    struct tm tmStruct;

    if (getLocalTime(&tmStruct, 1)) {
        rtc.hour = tmStruct.tm_hour;
        rtc.min = tmStruct.tm_min;
        rtc.sec = tmStruct.tm_sec;
        rtc.year = tmStruct.tm_year + RTC_YEAR_BEGIN;
        rtc.mon = tmStruct.tm_mon + 1;
        rtc.mday = tmStruct.tm_mday;
        rtc.wday = tmStruct.tm_wday;
        // RTC_TAG_CONSOLE("Get Time: %s", toString(&rtc).c_str());
    }

    return rtc;
}

uint32_t ESPTimeSystem::hhMMssGet() {
    esp_rtc_time_t rtc = get();
    return hhmmssFormat(&rtc);
}

uint32_t ESPTimeSystem::ddmmyyGet() {
    esp_rtc_time_t rtc = get();
    return ddmmyyFormat(&rtc);
}

/** 15:22:13 is formated with numerical value 152213*/
uint32_t ESPTimeSystem::hhmmssFormat(esp_rtc_time_t *rtc) {
    uint32_t t = rtc->hour * 10000 + rtc->min * 100 + rtc->sec;
    return t;
}

/** 15/03/2021 is formated with numerical value 150321*/
uint32_t ESPTimeSystem::ddmmyyFormat(esp_rtc_time_t *rtc) {
    uint32_t d = rtc->mday * 10000 + rtc->mon * 100 + rtc->year % 100;
    return d;
}

uint32_t ESPTimeSystem::yyyymmddFormat(esp_rtc_time_t *rtc) {
    //yyyymmdd
    uint32_t d = rtc->year * 10000 + rtc->mon * 100 + rtc->mday;
    return d;
}

/**
 * @brief Convert number hhmmss to string format hh:MM:ss
 * @param in n : hhmmss
 * @return string hh:MM:ss
*/
String ESPTimeSystem::hhmmssStr(uint32_t n) {
    int hour, min, sec;
    hour = n / 10000;
    min = (n % 10000) / 100;
    sec = n % 100;

    char str_time[9];
    snprintf(str_time, 9, "%02u:%02u:%02u", hour, min, sec);

    return String(str_time);
}

/**
 * @brief Convert number ddmmyy to string format dd/mm/20yy
 * @param in n : ddmmyy
 * @return string dd/mm/20yy
*/
String ESPTimeSystem::ddmmyyStr(uint32_t n) {
    int mday, month, year;
    mday = n / 10000;
    month = (n % 10000) / 100;
    year = n % 100;

    char str_date[11];
    snprintf(str_date, 11, "%02u/%02u/20%02u", mday, month, year);

    return String(str_date);
}

/**
 * @brief Convert number ddmmyy to string format 20yy/mm/dd
 * @param in n : ddmmyy
 * @return string 20yy/mm/dd
*/
String ESPTimeSystem::yyyymmddStr(uint32_t n) {
    int mday, month, year;
    mday = n / 10000;
    month = (n % 10000) / 100;
    year = n % 100;

    char str_date[11];
    snprintf(str_date, 11, "20%02u/%02u/%02u", year, month, mday);

    return String(str_date);
}

time_t ESPTimeSystem::nowFromDdmmyyhhmmss(uint32_t ddmmyy, uint32_t hhmmss) {
    esp_rtc_time_t rtc;
    rtc.hour = hhmmss / 10000;
    rtc.min = (hhmmss % 10000) / 100;
    rtc.sec = hhmmss % 100;

    rtc.wday = 0;
    rtc.mday = ddmmyy / 10000;
    rtc.mon = (ddmmyy % 10000) / 100;
    rtc.year = ddmmyy % 100 + 2000;

    return makeNowFromRtc(&rtc);
}

/* "Thu Jan 25 2018 19:39:48 GMT+0700 (SE Asia Standard Time)" */
bool ESPTimeSystem::GMTStringUpdate(const char *rtc_web, level_update_t level) {
    esp_rtc_time_t rtc = {0};

    /** max length is 3 characters and 1 null terminal character */
    char wdayStr[4], monthStr[4];

    uint8_t field;
    bool result = false;

    RTC_FUNCTION_TAG_CONSOLE("IN");

    RTC_TAG_CONSOLE("String input: %s", rtc_web);

    /** %3s for wdayStr[4] and monthStr[4] */
    field = sscanf(rtc_web, "%3s %3s %u %u %u:%u:%u",
                   wdayStr, monthStr, &rtc.mday, &rtc.year, &rtc.hour, &rtc.min, &rtc.sec);

    RTC_TAG_CONSOLE("parsed number of field: %u", field);

    constexpr uint8_t FIELD_NUM_EXPECTED = 7;
    if (FIELD_NUM_EXPECTED == field) {
        RTC_TAG_CONSOLE("Number of filed expected true !");

        constexpr uint8_t MONTH_BEGIN_NUM = 1;
        constexpr uint8_t MONTH_END_NUM = 12;

        /** What is the month? */
        for (rtc.mon = MONTH_BEGIN_NUM; rtc.mon <= MONTH_END_NUM; ++rtc.mon) {
            if (!strcmp_P(monthStr, mon_list[rtc.mon])) {
                RTC_TAG_CONSOLE("The month is %u", rtc.mon);
                break;
            }
        }

        /** Update source time level */
        if (_rtcSource < level_update_t::ESP_RTC_SNTP_UPDATE) {
            RTC_TAG_CONSOLE("Allowed to update source time level");
            _rtcSource = level;

            set(&rtc);
            RTC_TAG_CONSOLE("Time: %s", toString().c_str());
        }
        else {
            RTC_TAG_CONSOLE("Rejected update source time, "
            "because the level is lowest priority");
        }
        result = true;
    }

    RTC_FUNCTION_TAG_CONSOLE("OUT");
    return result;
}

// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Time/SimpleTime/SimpleTime.ino
void ESPTimeSystem::setTimeZone(long offset, int daylight) {
    char cst[17] = {0};
    char cdt[17] = "DST";
    char tz[33] = {0};

    if(offset % 3600) {
        sprintf(cst, "UTC%ld:%02u:%02u", offset / 3600, abs((offset % 3600) / 60), abs(offset % 60));
    } else {
        sprintf(cst, "UTC%ld", offset / 3600);
    }
    if(daylight != 3600){
        long tz_dst = offset - daylight;
        if(tz_dst % 3600) {
            sprintf(cdt, "DST%ld:%02u:%02u", tz_dst / 3600, abs((tz_dst % 3600) / 60), abs(tz_dst % 60));
        } else {
            sprintf(cdt, "DST%ld", tz_dst / 3600);
        }
    }
    sprintf(tz, "%s%s", cst, cdt);
    RTC_TAG_CONSOLE("tz: %s", tz);
    setenv("TZ", tz, 1);
    tzset();
}

void ESPTimeSystem::setTzTime(const char* tz) {
    RTC_TAG_CONSOLE("tz: %s", tz);
    setenv("TZ", tz, 1);
    tzset();
}

const char* ESPTimeSystem::printSourceUpdate(level_update_t level)
  {
    const char *str;
    switch (level)
    {
    case ESP_RTC_NON_UPDATE:
      str = (const char *)"ESP_RTC_NON_UPDATE";
      break;

    case ESP_RTC_WEB_UPDATE:
      str = (const char *)"ESP_RTC_WEB_UPDATE";
      break;

    case ESP_RTC_SNTP_UPDATE:
      str = (const char *)"ESP_RTC_SNTP_UPDATE";
      break;

    default:
      str = (const char *)"RTC_UNKNOW_UPDATE";
      break;
    }

    return (const char *)str;
  }

/* RtcFileHandler object handle -------------------------------------------------*/
RtcFileHandler::RtcFileHandler(FS &fs) : _fs(&fs) {}

RtcFileHandler::~RtcFileHandler() {}

void RtcFileHandler::save(esp_rtc_time_t *rtc) {
    File fs_handle;
    DynamicJsonBuffer djBuffer;
    JsonObject &root = djBuffer.createObject();
    root["year"].set(rtc->year);
    root["mon"].set(rtc->mon);
    root["mday"].set(rtc->mday);
    root["hour"].set(rtc->hour);
    root["min"].set(rtc->min);
    root["sec"].set(rtc->sec);
    root["wday"].set(rtc->wday);

    RTC_DATA_TAG_CONSOLE("Json created: ");
    root.prettyPrintTo(RTC_DATA_CONSOLE_PORT);

    fs_handle = _fs->open(ESP_RTC_FILE_PATH, "w");
    root.prettyPrintTo(fs_handle);
    fs_handle.close();
    RTC_DATA_TAG_CONSOLE("save succeed!");
}

bool RtcFileHandler::sync(esp_rtc_time_t *rtc) {
    File fs_handle;
    if (!_fs->exists(ESP_RTC_FILE_PATH)) {
        RTC_DATA_TAG_CONSOLE("File is not exist");
        /** Don't need to create new file default
         * Because the next start up won't ever update Rtc system with wrong time
        */
        return false;
    }

    fs_handle = _fs->open(ESP_RTC_FILE_PATH, "r");

    DynamicJsonBuffer djBuffer;
    JsonObject &root = djBuffer.parseObject(fs_handle);
    fs_handle.close();

    if (!root.success()) {
        RTC_DATA_TAG_CONSOLE("JSON parsing failed!");
        return false;
    }

    rtc->year = root["year"].as<int>();
    rtc->mon = root["mon"].as<int>();
    rtc->mday = root["mday"].as<int>();
    rtc->hour = root["hour"].as<int>();
    rtc->min = root["min"].as<int>();
    rtc->sec = root["sec"].as<int>();
    rtc->wday = root["wday"].as<int>();
    RTC_DATA_TAG_CONSOLE("Sync succeed!");
    return true;
}

void RtcFileHandler::remove(void) {
    if (_fs->remove(ESP_RTC_FILE_PATH)) {
        RTC_DATA_TAG_CONSOLE("- %s file is deleted\r\n", ESP_RTC_FILE_PATH);
    }
    else {
        RTC_DATA_TAG_CONSOLE("- %s delete failed\r\n", ESP_RTC_FILE_PATH);
    }
}

ESPTimeSystem ESPTime;
