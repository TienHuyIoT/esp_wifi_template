/*
  TimeAlarms.cpp - Arduino Time alarms for use with Time library
  Copyright (c) 2008-2011 Michael Margolis.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
 */

 /*
  2 July 2011 - replaced alarm types implied from alarm value with enums to make trigger logic more robust
              - this fixes bug in repeating weekly alarms - thanks to Vincent Valdy and draythomp for testing
*/

#include "THIoT_ESPTimeAlarms.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#define ALARM_TAG_CONSOLE(...)      SERIAL_TAG_LOGI("[ALARM]", __VA_ARGS__)
#define TIME_ALARM_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[TIME_ALARM]", __VA_ARGS__)
#define TIME_ALARM_TAG_LOG(...)     //FS_TAG_LOGI("[TIME_ALARM]", __VA_ARGS__)

#define IS_ONESHOT  true   // constants used in arguments to create method
#define IS_REPEAT   false
   
   
//**************************************************************
namespace espTimeAlarm {

//* Alarm Class Constructor
AlarmClass::AlarmClass()
{
  Mode.isEnabled = Mode.isOneShot = 0;
  Mode.alarmType = dtNotAllocated;
  value = nextTrigger = 0;
  onTickHandler = NULL;  // prevent a callback until this pointer is explicitly set
}

//**************************************************************
//* Private Methods

void AlarmClass::updateNextTrigger()
{
  if (Mode.isEnabled) {
    time_t now = time(nullptr);
    if (dtIsAlarm(Mode.alarmType) && nextTrigger <= now) {
      // update alarm if next trigger is not yet in the future
      if (Mode.alarmType == dtExplicitAlarm) {
        // is the value a specific date and time in the future
        nextTrigger = value;  // yes, trigger on this value
      } else if (Mode.alarmType == dtDailyAlarm) {
        //if this is a daily alarm
        if (value + previousMidnight(now) <= now) {
          // if time has passed then set for tomorrow
          // Can use the condition if (now == nextTrigger)
          // But it is a very strict comparison.
          // So using abs instead of that
          if (abs(now - nextTrigger) <= 2) {
            nextTrigger += SECS_PER_DAY;
          }
          else {
            // This case is only for the first time initiation
            nextTrigger = value + nextMidnight(now);
          }
        } else {
          // set the date to today and add the time given in value
          // This case is only for the first time initiation
          nextTrigger = value + previousMidnight(now);
        }
      } else if (Mode.alarmType == dtWeeklyAlarm) {
        // if this is a weekly alarm
        if ((value + previousSunday(now)) <= now) {
          // if day has passed then set for the next week.
          // Can use the condition if (now == nextTrigger)
          // But it is a very strict comparison.
          // So using abs instead of that
          if (abs(now - nextTrigger) <= 2) {
            nextTrigger += SECS_PER_WEEK;
          }
          else {
            // This case is only for the first time initiation
            nextTrigger = value + nextSunday(now);
          }
        } else {
          // set the date to this week today and add the time given in value
          // This case is only for the first time initiation
          nextTrigger = value + previousSunday(now);
        }
      } else {
        // its not a recognized alarm type - this should not happen
        Mode.isEnabled = false;  // Disable the alarm
      }
    }
    if (Mode.alarmType == dtTimer) {
      // its a timer
      nextTrigger = now + value;  // add the value to previous time (this ensures delay always at least Value seconds)
    }
  }
}

//**************************************************************
//* Time Alarms Public Methods

ESPTimeAlarmClass::ESPTimeAlarmClass()
{
  isServicing = false;
  for(uint8_t id = 0; id < dtNBR_ALARMS; id++) {
    freeID(id);   // ensure all Alarms are cleared and available for allocation
  }
}

/**
 * GMT +7
 * Friday 2023-11-10 22:01:36 UTC
 * H = 4, M = 0, S = 0
 * t1 = 1699628496, t2 = 1699563600, diff = -10800
 * This is a special case.
 * So the function updateNextTrigger() has a little bit update to fix
 * nextTrigger value expected
*/
time_t ESPTimeAlarmClass::AlarmHMS(int H, int M, int S) {
    time_t t1,t2;	
    struct tm tma;
    time(&t1);
    localtime_r(&t1, &tma);
    TIME_ALARM_TAG_CONSOLE("tm_hour = %u, tm_min = %u, tm_sec = %u, ",
                           tma.tm_hour, tma.tm_min, tma.tm_sec);
    tma.tm_hour = H;
    tma.tm_min = M;
    tma.tm_sec = S;
    t2 = mktime(&tma);
    time_t diff = t2-previousMidnight(t1);
    TIME_ALARM_TAG_CONSOLE("H = %u, M = %u, S = %u, ", H, M, S);
    TIME_ALARM_TAG_CONSOLE("t1 = %d, t2 = %d, diff = %d", t1, t2, diff);
    
    diff %= SECS_PER_DAY;
    return diff;
}

void ESPTimeAlarmClass::enable(AlarmID_t ID)
{
  if (isAllocated(ID)) {
    if (( !(dtUseAbsoluteValue(Alarm[ID].Mode.alarmType) && (Alarm[ID].value == 0)) ) && (Alarm[ID].onTickHandler != NULL)) {
      // only enable if value is non zero and a tick handler has been set
      // (is not NULL, value is non zero ONLY for dtTimer & dtExplicitAlarm
      // (the rest can have 0 to account for midnight))
      Alarm[ID].Mode.isEnabled = true;
      Alarm[ID].updateNextTrigger(); // trigger is updated whenever  this is called, even if already enabled
    } else {
      Alarm[ID].Mode.isEnabled = false;
    }
  }
}

void ESPTimeAlarmClass::disable(AlarmID_t ID)
{
  if (isAllocated(ID)) {
    Alarm[ID].Mode.isEnabled = false;
  }
}

// write the given value to the given alarm
void ESPTimeAlarmClass::write(AlarmID_t ID, time_t value)
{
  if (isAllocated(ID)) {
    Alarm[ID].value = value;  //note: we don't check value as we do it in enable()
    Alarm[ID].nextTrigger = 0; // clear out previous trigger time (see issue #12)
    enable(ID);  // update trigger time
  }
}

// return the value for the given alarm ID
time_t ESPTimeAlarmClass::read(AlarmID_t ID) const
{
  if (isAllocated(ID)) {
    return Alarm[ID].value ;
  } else {
    return dtINVALID_TIME;
  }
}

// return the alarm type for the given alarm ID
dtAlarmPeriod_t ESPTimeAlarmClass::readType(AlarmID_t ID) const
{
  if (isAllocated(ID)) {
    return (dtAlarmPeriod_t)Alarm[ID].Mode.alarmType ;
  } else {
    return dtNotAllocated;
  }
}

void ESPTimeAlarmClass::freeID(AlarmID_t ID)
{
  if (isAllocated(ID)) {
    Alarm[ID].Mode.isEnabled = false;
    Alarm[ID].Mode.alarmType = dtNotAllocated;
    Alarm[ID].onTickHandler = NULL;
    Alarm[ID].value = 0;
    Alarm[ID].nextTrigger = 0;
  }
}

// returns the number of allocated timers
uint8_t ESPTimeAlarmClass::count() const
{
  uint8_t c = 0;
  for(uint8_t id = 0; id < dtNBR_ALARMS; id++) {
    if (isAllocated(id)) c++;
  }
  return c;
}

// returns true only if id is allocated and the type is a time based alarm, returns false if not allocated or if its a timer
bool ESPTimeAlarmClass::isAlarm(AlarmID_t ID) const
{
  return( isAllocated(ID) && dtIsAlarm(Alarm[ID].Mode.alarmType) );
}

// returns true if this id is allocated
bool ESPTimeAlarmClass::isAllocated(AlarmID_t ID) const
{
  return (ID < dtNBR_ALARMS && Alarm[ID].Mode.alarmType != dtNotAllocated);
}

// returns the currently triggered alarm id
// returns dtINVALID_ALARM_ID if not invoked from within an alarm handler
AlarmID_t ESPTimeAlarmClass::getTriggeredAlarmId() const
{
  if (isServicing) {
    return servicedAlarmId;  // new private data member used instead of local loop variable i in serviceAlarms();
  } else {
    return dtINVALID_ALARM_ID; // valid ids only available when servicing a callback
  }
}

// following functions are not Alarm ID specific.
void ESPTimeAlarmClass::delay(unsigned long ms)
{
  unsigned long start = millis();
  while (millis() - start  <= ms) {
    serviceAlarms();
  }
}

void ESPTimeAlarmClass::waitForDigits( uint8_t Digits, dtUnits_t Units)
{
  while (Digits != getDigitsNow(Units)) {
    serviceAlarms();
  }
}

void ESPTimeAlarmClass::waitForRollover( dtUnits_t Units)
{
  // if its just rolled over than wait for another rollover
  while (getDigitsNow(Units) == 0) {
    serviceAlarms();
  }
  waitForDigits(0, Units);
}

uint8_t ESPTimeAlarmClass::getDigitsNow( dtUnits_t Units) const
{
  time_t now = time(nullptr);
  struct tm *NOW = localtime(&now);
  
  if (Units == dtSecond) return NOW->tm_sec;
  if (Units == dtMinute) return NOW->tm_min;
  if (Units == dtHour) return NOW->tm_hour;
  if (Units == dtDay) return NOW->tm_wday;
  return 255;  // This should never happen
}

//returns isServicing
bool ESPTimeAlarmClass::getIsServicing() const
{
  return isServicing;
}

//***********************************************************
//* Private Methods

void ESPTimeAlarmClass::serviceAlarms()
{
  if (!isServicing) {
    time_t now = time(nullptr);
    isServicing = true;
    for (servicedAlarmId = 0; servicedAlarmId < dtNBR_ALARMS; servicedAlarmId++) {
      if (Alarm[servicedAlarmId].Mode.isEnabled && (now >= Alarm[servicedAlarmId].nextTrigger)) {
        OnTick_t TickHandler = Alarm[servicedAlarmId].onTickHandler;
        if (Alarm[servicedAlarmId].Mode.isOneShot) {
          freeID(servicedAlarmId);  // free the ID if mode is OnShot
        } else {
          Alarm[servicedAlarmId].updateNextTrigger();
        }
        if (TickHandler != NULL) {
          TickHandler();     // call the handler
        }
      }
    }
    isServicing = false;
  }
}

// returns the absolute time of the next scheduled alarm, or 0 if none
time_t ESPTimeAlarmClass::getNextTrigger() const
{
  time_t nextTrigger = (time_t)0xffffffff;  // the max time value

  for (uint8_t id = 0; id < dtNBR_ALARMS; id++) {
    if (isAllocated(id)) {
      if (Alarm[id].nextTrigger <  nextTrigger) {
        nextTrigger = Alarm[id].nextTrigger;
      }
    }
  }
  return nextTrigger == (time_t)0xffffffff ? 0 : nextTrigger;
}

time_t ESPTimeAlarmClass::getNextTrigger(AlarmID_t ID) const
{
  if (isAllocated(ID)) {
    return Alarm[ID].nextTrigger;
  } else {
    return 0;
  }
}

// attempt to create an alarm and return true if successful
AlarmID_t ESPTimeAlarmClass::create(time_t value, OnTick_t onTickHandler, uint8_t isOneShot, dtAlarmPeriod_t alarmType)
{
  time_t now = time(nullptr);
  if ( ! ( (dtIsAlarm(alarmType) && now < SECS_PER_YEAR) || (dtUseAbsoluteValue(alarmType) && (value == 0)) ) ) {
    // only create alarm ids if the time is at least Jan 1 1971
    for (uint8_t id = 0; id < dtNBR_ALARMS; id++) {
      if (Alarm[id].Mode.alarmType == dtNotAllocated) {
        // here if there is an Alarm id that is not allocated
        Alarm[id].onTickHandler = onTickHandler;
        Alarm[id].Mode.isOneShot = isOneShot;
        Alarm[id].Mode.alarmType = alarmType;
        Alarm[id].value = value;
        enable(id);
        return id;  // alarm created ok
      }
    }
  }
  return dtINVALID_ALARM_ID; // no IDs available or time is invalid
}

} // namespace espTimeAlarm