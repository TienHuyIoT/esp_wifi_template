#include <Esp.h>
#include <ArduinoJson.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPSoftReset.h"
#include "THIoT_ESPCalendarSchedule.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define CALENDAR_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[CALENDAR]", __VA_ARGS__)
#define CALENDAR_FILE_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[CALENDAR_FILE]", __VA_ARGS__)

#define CALENDAR_TAG_LOG(...) FS_TAG_LOGI("[CALENDAR]", __VA_ARGS__)

/** @brief [type, arg, wday, hour, minute, second, enable]
 * @param type: typing handler: reset or execute a function
 * @param arg: the argument for the type handler
 * @param wday: 0 (daily), 1-7 respective Sun-Sat
 * @param hour: 0-23
 * @param minute: 0-59
 * @param second: 0-59
 * @param enable: 0: disable, 1: enable
 * 
 * Ex: [0,3000,0,04,0,0,0] 
 * Type: DAILY_RESET_TRIGGER_TYPE
 * Arg: 3000 used for timeout reset
 * wday: 0 mean's that daily event espTimeAlarm::dowInvalid
 * hour, min, sec: 04:00:00
 * enable: 0 disable
*/
const char calendarScheduleDefault[] PROGMEM = R"=====(
[
    [0,3000,0,04,0,0,1],
    [0,3000,0,12,0,0,1],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0]
]
)=====" ;

ESPCalendarSchedule::ESPCalendarSchedule(/* args */)
: _timeAlarm(), CalendarScheduleFileHandler(NAND_FS_SYSTEM) {
    _isLoad = false;
}

/**
 * @brief trigger load schedule after timeout
*/
void ESPCalendarSchedule::begin(uint32_t timeout) {
    /**
     * Has to be check to avoid updating the ticker due to its running
    */
    if (_isLoad) {
        CALENDAR_TAG_CONSOLE("Already loaded");
        return;
    }
    /**Creation timeout waiting for Sntp first times update
     * This way makes sure the system time correct when loading schedule.
    */
    ticker_once(&_alarmTick, timeout, [](void *arg){
        ESPCalendarSchedule *handler = (ESPCalendarSchedule*)arg;
        handler->_loadSchedule();
    }, this);
}

void ESPCalendarSchedule::_loadSchedule() {
    if (_isLoad) {
        CALENDAR_TAG_CONSOLE("Schedule is running");
        return;
    }
    _isLoad = true;
    CalendarScheduleFileHandler::load();
    CalendarScheduleFileHandler::calendar_t *calendar;
    for (int i = 0; i < CALENDAR_SCHEDULE_ELEMENT_MAX; ++i) {
        calendar = &_calendar[i];
        if (calendar->enable) {
            if (dowInvalid == calendar->wday) {
                CALENDAR_TAG_CONSOLE("Daily at %02u:%02u:%02u", calendar->hour, calendar->minute, calendar->second);
                  /**trigger daily at given time of day
                   * TimeAlarm lib shall be load system time and calculate the updateNextTrigger
                   * If the current time < this alarm time: Event shall be trigger in day
                   * Else the Event shall be trigger in the next day.
                  */
                calendar->alarmID = _timeAlarm.alarmRepeat(calendar->hour, calendar->minute, calendar->second,
                                       std::bind(&ESPCalendarSchedule::_execution, this, calendar));
                CALENDAR_TAG_CONSOLE("NextTrigger[%u] %s",
                                     calendar->alarmID,
                                     ESPTime.toString(_timeAlarm.getNextTrigger(calendar->alarmID)).c_str());
                CALENDAR_TAG_LOG("NextTrigger[%u] %s",
                                     calendar->alarmID,
                                     ESPTime.toString(_timeAlarm.getNextTrigger(calendar->alarmID)).c_str());
            }
            else {
                CALENDAR_TAG_CONSOLE("Weekly at wday(%u) %02u:%02u:%02u", calendar->wday, calendar->hour, calendar->minute, calendar->second);
                  // trigger weekly at a specific day and time
                calendar->alarmID = _timeAlarm.alarmRepeat(calendar->wday, calendar->hour, calendar->minute, calendar->second,
                                       std::bind(&ESPCalendarSchedule::_execution, this, calendar));
                CALENDAR_TAG_CONSOLE("NextTrigger[%u] %s",
                                     calendar->alarmID,
                                     ESPTime.toString(_timeAlarm.getNextTrigger(calendar->alarmID)).c_str()); 
                CALENDAR_TAG_LOG("NextTrigger[%u] %s",
                                     calendar->alarmID,
                                     ESPTime.toString(_timeAlarm.getNextTrigger(calendar->alarmID)).c_str()); 
            }
        }
    }

    ticker_attach_ms(&_alarmTick, 1000, [](void *arg){
        ESPCalendarSchedule *handler = (ESPCalendarSchedule*)arg;
        handler->_timeAlarm.delay(0);
    }, this);
}

void ESPCalendarSchedule::_execution(calendar_t* calendar) {
#if (0)
    /**
     * Don't need to compare due to system reset will be keep 
     * the system's rtc
    */
    if (ESPTime.getSourceUpdate() == ESPTimeSystem::ESP_RTC_NON_UPDATE) {
        CALENDAR_FILE_TAG_CONSOLE("Alarm enable failure");
        return;
    }
#endif
    CALENDAR_TAG_CONSOLE("Schedule[%u] %u-%u at %02u:%02u:%02u",
                         calendar->alarmID,
                         calendar->type,
                         calendar->arg,
                         calendar->hour,
                         calendar->minute,
                         calendar->second);
    
    if (DAILY_RESET_TRIGGER_TYPE == calendar->type) {
        uint32_t timeout = calendar->arg;
        CALENDAR_TAG_LOG("Daily[%u] restart at %02u:%02u:%02u",
                         calendar->alarmID,
                         calendar->hour,
                         calendar->minute,
                         calendar->second);
        
        SOFTReset.enable(timeout, ESPSoftReset::SCHEDULE_DAILY_TYPE);
    }

    if (WEEKLY_RESET_TRIGGER_TYPE == calendar->type) {
        uint32_t timeout = calendar->arg;
        CALENDAR_TAG_LOG("Weekly[%u] restart at wday(%u) %02u:%02u:%02u",
                         calendar->alarmID,
                         calendar->wday,
                         calendar->hour,
                         calendar->minute,
                         calendar->second);
        SOFTReset.enable(timeout, ESPSoftReset::SCHEDULE_WEEKLY_TYPE);
    }

    CALENDAR_TAG_CONSOLE("NextTrigger[%u] %s",
                         calendar->alarmID,
                         ESPTime.toString(_timeAlarm.getNextTrigger(calendar->alarmID)).c_str());
    CALENDAR_TAG_LOG("NextTrigger[%u] %s",
                     calendar->alarmID,
                     ESPTime.toString(_timeAlarm.getNextTrigger(calendar->alarmID)).c_str());
}

/* CalendarScheduleFileHandler object handle -------------------------------------------------*/
CalendarScheduleFileHandler::CalendarScheduleFileHandler(FS &fs) 
: _fs(&fs){}

CalendarScheduleFileHandler::~CalendarScheduleFileHandler() {}

void CalendarScheduleFileHandler::load(FS* fs) {
    if (fs != nullptr) {
        _fs = fs;
    }
    syncFromFileSystem();
}

void CalendarScheduleFileHandler::saveLatency(int timeout) {
    ticker_once(&_saveTicker, timeout, [](void *arg){
        CalendarScheduleFileHandler *handler = (CalendarScheduleFileHandler*)arg;
        handler->saveToFileSystem();
    }, this);
}

void CalendarScheduleFileHandler::saveToFileSystem() {
    const size_t capacity = JSON_ARRAY_SIZE(CALENDAR_SCHEDULE_ELEMENT_MAX) + CALENDAR_SCHEDULE_ELEMENT_MAX*JSON_ARRAY_SIZE(7);
    DynamicJsonBuffer jsonBuffer(capacity);

    JsonArray& root = jsonBuffer.createArray();
    for (int i = 0; i < CALENDAR_SCHEDULE_ELEMENT_MAX; ++i)
    {
        JsonArray& element = root.createNestedArray();
        element.add(_calendar[i].type);
        element.add(_calendar[i].arg);
        element.add((uint8_t)_calendar[i].wday);
        element.add(_calendar[i].hour);
        element.add(_calendar[i].minute);
        element.add(_calendar[i].second);
        element.add((uint8_t)_calendar[i].enable);
    }

    File fs_handle;
    fs_handle = _fs->open(CALENDAR_SCHEDULE_PATH, "w");
    root.prettyPrintTo<File>(fs_handle);
    fs_handle.close();

    CALENDAR_FILE_TAG_CONSOLE("Save to file system");
}

void CalendarScheduleFileHandler::syncFromFileSystem() {
    File fs_handle;

    if (!_fs->exists(CALENDAR_SCHEDULE_PATH)) {        
        fs_handle = _fs->open(CALENDAR_SCHEDULE_PATH, "w");
        fs_handle.printf_P(calendarScheduleDefault);
        fs_handle.close();
        CALENDAR_FILE_TAG_CONSOLE("Init data succeed!");
    }

    fs_handle = _fs->open(CALENDAR_SCHEDULE_PATH, "r");    
    
    const size_t capacity = CALENDAR_SCHEDULE_ELEMENT_MAX*JSON_ARRAY_SIZE(7) + JSON_ARRAY_SIZE(CALENDAR_SCHEDULE_ELEMENT_MAX) + 50;
    DynamicJsonBuffer jsonBuffer(capacity);
    JsonArray& root = jsonBuffer.parseArray(fs_handle);
    fs_handle.close();

    if (!root.success()) {
        CALENDAR_FILE_TAG_CONSOLE("JSON parsing failed!");
        return;
    }

    for (int i = 0; i < CALENDAR_SCHEDULE_ELEMENT_MAX; ++i) {
        JsonArray& element  = root[i];
        _calendar[i].type   = element[CALENDAR_SCHEDULE_TYPE_JSON_ARRAY_INDEX].as<uint32_t>();
        _calendar[i].arg    = element[CALENDAR_SCHEDULE_ARG_JSON_ARRAY_INDEX].as<uint32_t>();
        _calendar[i].wday   = (timeDayOfWeek_t)element[CALENDAR_SCHEDULE_WDAY_JSON_ARRAY_INDEX].as<uint8_t>();
        _calendar[i].hour   = element[CALENDAR_SCHEDULE_HOUR_JSON_ARRAY_INDEX].as<uint8_t>();
        _calendar[i].minute = element[CALENDAR_SCHEDULE_MINUTE_JSON_ARRAY_INDEX].as<uint8_t>();
        _calendar[i].second = element[CALENDAR_SCHEDULE_SECOND_JSON_ARRAY_INDEX].as<uint8_t>();
        _calendar[i].enable = element[CALENDAR_SCHEDULE_ENABLE_JSON_ARRAY_INDEX].as<uint8_t>();
    }
    CALENDAR_FILE_TAG_CONSOLE("sync data succeed!");
}

ESPCalendarSchedule CalendarSchedule;
