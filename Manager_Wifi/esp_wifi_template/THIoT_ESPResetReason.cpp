#include "THIoT_ESPResetReason.h"
#include "THIoT_SerialTrace.h"

#define WTD_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[RESET REASON]", __VA_ARGS__)

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

void esp_print_reset_reason()
{
#ifdef ESP32
    WTD_TAG_CONSOLE("CPU0 reset reason: %s", get_reset_reason(rtc_get_reset_reason(0)).c_str());
    WTD_TAG_CONSOLE("CPU1 reset reason: %s", get_reset_reason(rtc_get_reset_reason(1)).c_str());
#elif defined(ESP8266)
    WTD_TAG_CONSOLE("CPU reset reason: %s", ESP.getResetReason().c_str());
#endif
    
}
