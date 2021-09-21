#include <time.h> // time() ctime()
#ifdef ESP8266
#include <sys/time.h> // struct timeval
#endif
#include <ArduinoJson.h>
#include "hth_esp_config.h"
#include "hth_serial_trace.h"
#include "hth_esp_sys_rtc.h"

#define RTC_CONSOLE_PORT CONSOLE_PORT
#define RTC_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)
#define RTC_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[RTC]", __VA_ARGS__)

#define RTC_DATA_CONSOLE_PORT CONSOLE_PORT
#define RTC_DATA_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)
#define RTC_DATA_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[RTC DATA]", __VA_ARGS__)

#define RTC_FILE_PATH ((const char*)"/rtc_info_file.txt")

const char *const mon_list[] PROGMEM = {
            "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

#ifdef ESP8266
static bool getLocalTime(struct tm *info, uint32_t ms)
{
    uint32_t start = millis();
    time_t now;
    while ((millis() - start) <= ms)
    {
        time(&now);
        localtime_r(&now, info);
        if (info->tm_year >= (2021 - 1900))
        {
            return true;
        }
        delay(10);
    }
    return false;
}
#endif

ESPTimeSystem::ESPTimeSystem(/* args */)
    : _rtcFile(new RtcFileHandler(NAND_FS_SYSTEM))
{
    _rtcSource = level_update_t::RTC_NON_UPDATE;
}

ESPTimeSystem::~ESPTimeSystem()
{
}

void ESPTimeSystem::load(void)
{
    struct tm tmStruct;
    rtc_time_t rtc;

    // Set timezone to vietnam Standard Time
    setenv("TZ", "CST-7", 1);
    tzset();

    if (!getLocalTime(&tmStruct, 1))
    {
        if (_rtcFile->sync(&rtc))
        {
            _rtcFile->remove();
            // rtc.year = 2020;
            // rtc.mon = 8;
            // rtc.mday = 2;
            // rtc.hour = 22;
            // rtc.min = 51;
            // rtc.sec = 0;
            // rtc.wday = 0; // sunday(0), sun(0) ... sat(6)
            RTC_TAG_CONSOLE("Update RTC from file system");
            set(&rtc);
            get(&rtc); // just verify time system
        }
        else
        {
            RTC_TAG_CONSOLE("Init RTC system");
            const char *sysTime = "Tue " __DATE__ " " __TIME__ " GMT";
            GMTStringUpdate(sysTime, level_update_t::RTC_NON_UPDATE);
        }
    }
    else
    {
        get(&rtc); // just verify time system
    }
}

void ESPTimeSystem::saveToFileSystem()
{
    rtc_time_t rtc;
    if (_rtcSource > level_update_t::RTC_NON_UPDATE)
    {
        RTC_TAG_CONSOLE("Save Time to File system");
        get(&rtc);
        _rtcFile->save(&rtc);
    }
}

void ESPTimeSystem::setSourceUpdate(level_update_t level)
{
    RTC_TAG_CONSOLE("Update the level: %s", printSourceUpdate(level));
    if (level >= _rtcSource)
    {
        RTC_TAG_CONSOLE("Access \r\n");
        _rtcSource = level;
    }
    else
    {
        RTC_TAG_CONSOLE("Deny \r\n");
    }
}

void ESPTimeSystem::setTime(rtc_time_t *rtc)
{
    rtc_time_t g_rtc;
    get(&g_rtc);
    g_rtc.hour = rtc->hour;
    g_rtc.min = rtc->min;
    g_rtc.sec = rtc->sec;
    set(&g_rtc);
}

void ESPTimeSystem::setDate(rtc_time_t *rtc)
{
    rtc_time_t g_rtc;
    get(&g_rtc);
    g_rtc.year = rtc->year;
    g_rtc.mon = rtc->mon;
    g_rtc.mday = rtc->mday;
    set(&g_rtc);
}

void ESPTimeSystem::set(rtc_time_t *rtc)
{
    struct timeval t_val;
    time_t t_now = 0;
    t_now = makeNowFromRtc(rtc);
    t_val = {.tv_sec = t_now};
    settimeofday(&t_val, NULL);
}

/* Convert t_now to rtc */
rtc_time_t ESPTimeSystem::makeRtcFromNow(time_t t_now)
{
    struct tm desired_tm;
    rtc_time_t rtc;
    localtime_r((time_t *)&t_now, &desired_tm);
    rtc.mon = desired_tm.tm_mon + 1;
    rtc.mday = desired_tm.tm_mday;
    rtc.year = desired_tm.tm_year + 1900;
    rtc.hour = desired_tm.tm_hour;
    rtc.min = desired_tm.tm_min;
    rtc.sec = desired_tm.tm_sec;
    rtc.wday = desired_tm.tm_wday;
    return rtc;
}

/* Convert rtc to now */
time_t ESPTimeSystem::makeNowFromRtc(rtc_time_t *rtc)
{
    struct tm desired_tm;
    time_t t_now = 0;
    memset(&desired_tm, 0, sizeof(struct tm));
    desired_tm.tm_mon = rtc->mon - 1;
    desired_tm.tm_mday = rtc->mday;
    desired_tm.tm_year = rtc->year - 1900;
    desired_tm.tm_hour = rtc->hour;
    desired_tm.tm_min = rtc->min;
    desired_tm.tm_sec = rtc->sec;
    desired_tm.tm_wday = rtc->wday;
    t_now = mktime(&desired_tm);
    return t_now;
}

char *ESPTimeSystem::printTimeFromNow(time_t t_now)
{
    return ctime(&t_now);
}

time_t ESPTimeSystem::toNow()
{
    rtc_time_t rtc;
    time_t t_now = 0;

    if (get(&rtc))
    {
        t_now = makeNowFromRtc(&rtc);
    }

    return t_now;
}

bool ESPTimeSystem::get(rtc_time_t *rtc)
{
    struct tm tmStruct;
    if (getLocalTime(&tmStruct, 1))
    {
        rtc->hour = tmStruct.tm_hour;
        rtc->min = tmStruct.tm_min;
        rtc->sec = tmStruct.tm_sec;
        rtc->year = tmStruct.tm_year + 1900;
        rtc->mon = tmStruct.tm_mon + 1;
        rtc->mday = tmStruct.tm_mday;
        rtc->wday = tmStruct.tm_wday;

        char buf[64];
        /** The same way using with esp32 
         * RTC_CONSOLE_PORT.println(&tmStruct, "\r\nTime: %A, %B %d %Y %H:%M:%S");
         * */
        strftime(buf, 64, "%A, %B %d %Y %H:%M:%S", &tmStruct);
        RTC_TAG_CONSOLE("Time: %s", buf);
        return true;
    }
    return false;
}

/** 15:22:13 is formated with numerical value 152213*/
uint32_t ESPTimeSystem::hhmmssFormat(rtc_time_t *rtc)
{
    uint32_t t = rtc->hour * 10000 + rtc->min * 100 + rtc->sec;
    return t;
}

/** 15/03/2021 is formated with numerical value 150321*/
uint32_t ESPTimeSystem::ddmmyyFormat(rtc_time_t *rtc)
{
    uint32_t d = rtc->mday * 10000 + rtc->mon * 100 + rtc->year % 100;
    return d;
}

/* "Thu Jan 25 2018 19:39:48 GMT+0700 (SE Asia Standard Time)" */
bool ESPTimeSystem::GMTStringUpdate(const char *rtc_web, level_update_t level)
{
    rtc_time_t rtc = {0};

    /** max length is 3 characters and 1 null terminal character */
    char wdayStr[4], monthStr[4];

    uint8_t field;
    bool result = false;

    RTC_TAG_CONSOLE("String input: %s", rtc_web);

    /** %3s for wdayStr[4] and monthStr[4] */
    field = sscanf(rtc_web, "%3s %3s %u %u %u:%u:%u",
                   wdayStr, monthStr, &rtc.mday, &rtc.year, &rtc.hour, &rtc.min, &rtc.sec);

    RTC_TAG_CONSOLE("parsed number of field: %u", field);

    constexpr uint8_t FIELD_NUM_EXPECTED = 7;
    if (FIELD_NUM_EXPECTED == field)
    {
        RTC_TAG_CONSOLE("Number of filed expected true !");

        constexpr uint8_t MONTH_BEGIN_NUM = 1;
        constexpr uint8_t MONTH_END_NUM = 12;

        /** What is the month? */
        for (rtc.mon = MONTH_BEGIN_NUM; rtc.mon <= MONTH_END_NUM; ++rtc.mon)
        {
            if (!strcmp_P(monthStr, mon_list[rtc.mon]))
            {
                RTC_TAG_CONSOLE("The month is %u", rtc.mon);
                break;
            }
        }

        /** Update source time level */
        if (_rtcSource < level_update_t::RTC_SNTP_UPDATE)
        {
            RTC_TAG_CONSOLE("Allowed to update source time level");
            _rtcSource = level;

            set(&rtc);
            get(&rtc);
        }
        else
        {
            RTC_TAG_CONSOLE("Rejected update source time, "
            "because the level is lowest priority");
        }
        result = true;
    }

    return result;
}

/* RtcFileHandler object handle -------------------------------------------------*/
RtcFileHandler::RtcFileHandler(fs::FS &fs)
    : _fs(&fs)
{
}

RtcFileHandler::~RtcFileHandler()
{
}

void RtcFileHandler::save(rtc_time_t *rtc)
{
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

    fs_handle = _fs->open(RTC_FILE_PATH, "w");
    root.prettyPrintTo(fs_handle);
    fs_handle.close();
    RTC_DATA_TAG_CONSOLE("save succeed!");
}

bool RtcFileHandler::sync(rtc_time_t *rtc)
{
    File fs_handle;
    if (!_fs->exists(RTC_FILE_PATH))
    {
        RTC_DATA_TAG_CONSOLE("File is not exist");
        /** Don't need to create new file default
         * Because the next start up won't ever update Rtc system with wrong time
        */
        return false;
    }

    fs_handle = _fs->open(RTC_FILE_PATH, "r");

    DynamicJsonBuffer djBuffer;
    JsonObject &root = djBuffer.parseObject(fs_handle);
    fs_handle.close();

    if (!root.success())
    {
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

void RtcFileHandler::remove(void)
{
    if (_fs->remove(RTC_FILE_PATH))
    {
        RTC_DATA_TAG_CONSOLE("- %s file is deleted\r\n", RTC_FILE_PATH);
    }
    else
    {
        RTC_DATA_TAG_CONSOLE("- %s delete failed\r\n", RTC_FILE_PATH);
    }
}

ESPTimeSystem ESPTime;
