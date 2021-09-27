#include "THIoT_ESPResetReason.h"
#include "THIoT_SerialTrace.h"

#ifdef ESP8266
extern "C" {
  #include "user_interface.h"
}
#endif

#define WTD_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[RESET REASON]", __VA_ARGS__)

#ifdef ESP32
static String get_reset_reason(RESET_REASON reason)
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
    default : buff = F("NO_MEAN");
  }
  return String(buff);
}
#endif

#ifdef ESP32
/**
 * @brief get reset reason type
 * @param [in] cpu_no the CPU number
 * @return The CPU reset reason type ref enum RESET_REASON
 * NO_MEAN                =  0,
 * POWERON_RESET          =  1,    Vbat power on reset
 * SW_RESET               =  3,    Software reset digital core
 * OWDT_RESET             =  4,    Legacy watch dog reset digital core
 * DEEPSLEEP_RESET        =  5,    Deep Sleep reset digital core
 * SDIO_RESET             =  6,    Reset by SLC module, reset digital core
 * TG0WDT_SYS_RESET       =  7,    Timer Group0 Watch dog reset digital core
 * TG1WDT_SYS_RESET       =  8,    Timer Group1 Watch dog reset digital core
 * RTCWDT_SYS_RESET       =  9,    RTC Watch dog Reset digital core
 * INTRUSION_RESET        = 10,     Instrusion tested to reset CPU
 * TGWDT_CPU_RESET        = 11,     Time Group reset CPU
 * SW_CPU_RESET           = 12,     Software reset CPU
 * RTCWDT_CPU_RESET       = 13,     RTC Watch dog Reset CPU
 * EXT_CPU_RESET          = 14,     for APP CPU, reseted by PRO CPU
 * RTCWDT_BROWN_OUT_RESET = 15,     Reset when the vdd voltage is not stable
 * RTCWDT_RTC_RESET       = 16      RTC Watch dog reset digital core and rtc module
*/
uint32_t esp_reset_reason(int cpu_no)
#elif defined(ESP8266)
/**
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
uint32_t esp_reset_reason()
#endif
{
#ifdef ESP32
  return rtc_get_reset_reason(cpu_no);
#elif defined(ESP8266)
  struct rst_info *rtc_info_ptr = system_get_rst_info();
  return rtc_info_ptr->reason;
#endif
}

String esp_reset_reason_str() {
  String reason = String();
#ifdef ESP32
  reason += "CPU0: ";
  reason += get_reset_reason(rtc_get_reset_reason(0));
  reason += "; CPU1: ";
  reason += get_reset_reason(rtc_get_reset_reason(1));
#elif defined(ESP8266)
  reason += "CPU: ";
  reason += ESP.getResetReason();
#endif
  return reason;
}
