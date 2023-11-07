#ifndef	__ESP_CALENDAR_SCHEDULE_H
#define __ESP_CALENDAR_SCHEDULE_H

#include <Arduino.h>
#include <functional>
#include "THIoT_PFTicker.h"
#include "THIoT_ESPTimeAlarms.h"

#define CALENDAR_SCHEDULE_PATH ((const char*)"/calendar_schedule.txt")
#define CALENDAR_SCHEDULE_ELEMENT_MAX   dtNBR_ALARMS

#define CALENDAR_SCHEDULE_TYPE_JSON_ARRAY_INDEX    0
#define CALENDAR_SCHEDULE_ARG_JSON_ARRAY_INDEX     1
#define CALENDAR_SCHEDULE_WDAY_JSON_ARRAY_INDEX    2
#define CALENDAR_SCHEDULE_HOUR_JSON_ARRAY_INDEX    3
#define CALENDAR_SCHEDULE_MINUTE_JSON_ARRAY_INDEX  4
#define CALENDAR_SCHEDULE_SECOND_JSON_ARRAY_INDEX  5
#define CALENDAR_SCHEDULE_ENABLE_JSON_ARRAY_INDEX  6

typedef std::function<void(uint32_t)> ResetHandler;
using namespace espTimeAlarm;
class CalendarScheduleFileHandler
{
    using File = fs::File;
    using FS = fs::FS;
    
public:
    typedef enum {
        DAILY_RESET_TRIGGER_TYPE = 0,
        WEEKLY_RESET_TRIGGER_TYPE
    } execution_trigger_t;

    typedef struct {
        uint32_t type;
        uint32_t arg;
        timeDayOfWeek_t wday;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        boolean enable;
    } calendar_t;

protected:
    FS *_fs;
    ticker_function_handle_t _saveTicker;
    calendar_t _calendar[CALENDAR_SCHEDULE_ELEMENT_MAX];
    calendar_t _nextObj;
    int _index;

private:
    void syncFromFileSystem();
    void saveToFileSystem();

public:
    CalendarScheduleFileHandler(FS &fs = NAND_FS_SYSTEM);
    ~CalendarScheduleFileHandler();

    void load(FS *fs = nullptr);
    void save() { saveToFileSystem(); }
    void saveLatency(int timeout = 2);
    void setDefault() { _fs->remove(CALENDAR_SCHEDULE_PATH); }

    timeDayOfWeek_t wdayGet(uint8_t No) {return _calendar[No].wday;}
    void wdaySet(uint8_t No, timeDayOfWeek_t wday) {
        _calendar[No].wday = wday;
        saveLatency();
    }
    uint8_t hourGet(uint8_t No) {return _calendar[No].hour;}
    void hourSet(uint8_t No, uint8_t hour) {
        _calendar[No].hour = hour;
        saveLatency();
    }
    uint8_t minuteGet(uint8_t No) {return _calendar[No].minute;}
    void minuteSet(uint8_t No, uint8_t minute) {
        _calendar[No].minute = minute;
        saveLatency();
    }
    uint8_t secondGet(uint8_t No) {return _calendar[No].second;}
    void secondSet(uint8_t No, uint8_t second) {
        _calendar[No].second = second;
        saveLatency();
    }
    uint8_t typeGet(uint8_t No) {return _calendar[No].type;}
    void typeSet(uint8_t No, uint8_t type) {
        _calendar[No].type = type;
        saveLatency();
    }
    boolean enableGet(uint8_t No) {return _calendar[No].enable;}
    void enableSet(uint8_t No, boolean enable) {
        _calendar[No].enable = enable;
        saveLatency();
    }
};

class ESPCalendarSchedule : public CalendarScheduleFileHandler
{
private:
    ESPTimeAlarmClass _timeAlarm;
    ticker_function_handle_t _alarmTick;
    boolean _isLoad;
    void _execution(calendar_t* calendar);
    void _loadSchedule();
public:
    ESPCalendarSchedule();
    ~ESPCalendarSchedule() {}
    void begin(uint32_t timeout = 30);
};

extern ESPCalendarSchedule CalendarSchedule;

#endif // __ESP_CALENDAR_SCHEDULE_H
