#include "THIoT_ESPResetReason.h"
#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_PFSerialTrace.h"

#ifdef ESP8266
extern "C" {
  #include "user_interface.h"
}
#endif

#define RST_REASON_TAG_CONSOLE(...)   //CONSOLE_TAG_LOGI("[RESET REASON]", __VA_ARGS__)

RTC_NOINIT_ATTR int noInitPowerOn;
static boolean forcePowerOn = false;
static uint8_t rReset[APP_CPU_NUM] = {0xFF, 0xFF};

#ifdef ESP32
#include <esp_system.h>
#define REASON_TIMER_WDT_SYS_RST  17
#define REASON_EXT_SYS_RST        99
static String get_reset_reason(int reason)
{
  const __FlashStringHelper* buff;
  switch ( reason)
  {
    case 1  : buff = F("Vbat power on reset");break;
    case 3  : buff = F("Software reset digital core");break;
    case 4  : buff = F("Legacy watch dog reset digital core");break;
    case 5  : buff = F("Deep Sleep reset digital core");break;
    case 6  : buff = F("Reset by SLC module, reset digital core");break;
    case 7  : buff = F("Timer Group0 Watch dog reset digital core");break;
    case 8  : buff = F("Timer Group1 Watch dog reset digital core");break;
    case 9  : buff = F("RTC Watch dog Reset digital core");break;
    case 10 : buff = F("Instrusion tested to reset CPU");break;
    case 11 : buff = F("Time Group reset CPU");break;
    case 12 : buff = F("Software reset CPU");break;
    case 13 : buff = F("RTC Watch dog Reset CPU");break;
    case 14 : buff = F("for APP CPU, reseted by PRO CPU");break;
    case 15 : buff = F("Reset when the vdd voltage is not stable");break;
    case 16 : buff = F("RTC Watch dog reset digital core and rtc module");break;
    case 17 : buff = F("Timer Watch dog Reset CPU");break;
    case 99 : buff = F("External system reset");break;
    default : buff = F("NO_MEAN");
  }
  return String(buff);
}
#endif

void esp_reset_reason_force_as_PowerON() {
  noInitPowerOn = POWER_ON_DEFAULT_NUM;
}

boolean is_esp_reset_reason_force_as_PowerON() {
  return forcePowerOn;
}

/**
 * @brief get reset reason type
 * @param [in] cpu_no the CPU number
 * @return The CPU reset reason type ref enum RESET_REASON
 * 
 * for ESP32
 * NO_MEAN                =  0,
 * POWERON_RESET          =  1,    Vbat power on reset
 * SW_RESET               =  3,    Software reset digital core
 * OWDT_RESET             =  4,    Legacy watch dog reset digital core
 * DEEPSLEEP_RESET        =  5,    Deep Sleep reset digital core
 * SDIO_RESET             =  6,    Reset by SLC module, reset digital core
 * TG0WDT_SYS_RESET       =  7,    Timer Group0 Watch dog reset digital core
 * TG1WDT_SYS_RESET       =  8,    Timer Group1 Watch dog reset digital core
 * RTCWDT_SYS_RESET       =  9,    RTC Watch dog Reset digital core
 * INTRUSION_RESET        = 10,    Instrusion tested to reset CPU
 * TGWDT_CPU_RESET        = 11,    Time Group reset CPU
 * SW_CPU_RESET           = 12,    Software reset CPU
 * RTCWDT_CPU_RESET       = 13,    RTC Watch dog Reset CPU
 * EXT_CPU_RESET          = 14,    for APP CPU, reseted by PRO CPU
 * RTCWDT_BROWN_OUT_RESET = 15,    Reset when the vdd voltage is not stable
 * RTCWDT_RTC_RESET       = 16,    RTC Watch dog reset digital core and rtc module
 * REASON_EXT_SYS_RST     = 99     Reset by external pin (not applicable for ESP32)
 * 
 * for ESP8266
 * @brief get reset reason type
 * @return The CPU reset reason type ref enum rst_reason
 * REASON_DEFAULT_RST      = 0,    normal startup by power on
 * REASON_WDT_RST          = 1,    hardware watch dog reset
 * REASON_EXCEPTION_RST    = 2,    exception reset, GPIO status won’t change
 * REASON_SOFT_WDT_RST     = 3,    software watch dog reset, GPIO status won’t change
 * REASON_SOFT_RESTART     = 4,    software restart ,system_restart , GPIO status won’t change
 * REASON_DEEP_SLEEP_AWAKE = 5,    wake up from deep-sleep
 * REASON_EXT_SYS_RST      = 6     external system reset
*/
int esp_reset_reason_core(int cpu_no)
{
  if (0xFF == rReset[cpu_no]) {
#ifdef ESP32
    rReset[cpu_no] = rtc_get_reset_reason(cpu_no);
    if (POWERON_RESET == rReset[cpu_no]) {
      if (POWER_ON_VALIDATE_NUM == noInitPowerOn) {
        rReset[cpu_no] = REASON_EXT_SYS_RST;
      }
    } else {
      /**
       * In case of update new firmware or force as PowerON reset from http API
      */
      if (POWER_ON_VALIDATE_NUM != noInitPowerOn) {
        rReset[cpu_no] = POWERON_RESET;
      }
      if (ESPWdt.isWdtReset() && APP_CPU1_ID == cpu_no) {
        rReset[cpu_no] = REASON_TIMER_WDT_SYS_RST;
      }
    }
#elif defined(ESP8266)
    struct rst_info *rtc_info_ptr = system_get_rst_info();
    rReset[cpu_no] = rtc_info_ptr->reason;
#endif
  }

  /**
   * In case of force as PowerON reset from http API
  */
  if (POWER_ON_DEFAULT_NUM == noInitPowerOn) {
    forcePowerOn = true;
  }
  
  noInitPowerOn = POWER_ON_VALIDATE_NUM; /* Magic number for the variable no init power on*/
  if (APP_CPU1_ID == cpu_no) {
    ESPWdt.wdtStatusReset();
  }
  return rReset[cpu_no];
}

String esp_reset_reason_str() {
  String reason = String();
#ifdef ESP32
  reason += "CPU0: ";
  reason += get_reset_reason(esp_reset_reason_core(PRO_CPU0_ID));
  reason += "; CPU1: ";
  reason += get_reset_reason(esp_reset_reason_core(APP_CPU1_ID));
#elif defined(ESP8266)
  reason += "CPU: ";
  reason += ESP.getResetReason();
#endif
  return reason;
}

String esp_reset_reason_str(int type) {
  return get_reset_reason(type);
}
