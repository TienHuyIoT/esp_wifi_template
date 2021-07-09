#if (ESP_IDF_VERSION_MAJOR >= 4) // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include <esp32/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32S2
#include <esp32s2/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32C3
#include <esp32c3/rom/rtc.h>
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/rtc.h"
#endif

#include "log_report.h"
#include "console_dbg.h"

#define REASON_RESET_PORT CONSOLE_PORT
#define REASON_RESET_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)

extern void log_report(uint8_t log_id, char *p_log);

void print_reset_reason(RESET_REASON reason)
{
  char buff[60];
  switch ( reason)
  {
    case 1 : sprintf (buff, "POWERON_RESET");break;          /**<1,  Vbat power on reset*/
    case 3 : sprintf (buff, "SW_RESET");break;               /**<3,  Software reset digital core*/
    case 4 : sprintf (buff, "OWDT_RESET");break;             /**<4,  Legacy watch dog reset digital core*/
    case 5 : sprintf (buff, "DEEPSLEEP_RESET");break;        /**<5,  Deep Sleep reset digital core*/
    case 6 : sprintf (buff, "SDIO_RESET");break;             /**<6,  Reset by SLC module, reset digital core*/
    case 7 : sprintf (buff, "TG0WDT_SYS_RESET");break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8 : sprintf (buff, "TG1WDT_SYS_RESET");break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9 : sprintf (buff, "RTCWDT_SYS_RESET");break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10 : sprintf (buff, "INTRUSION_RESET");break;       /**<10, Instrusion tested to reset CPU*/
    case 11 : sprintf (buff, "TGWDT_CPU_RESET");break;       /**<11, Time Group reset CPU*/
    case 12 : sprintf (buff, "SW_CPU_RESET");break;          /**<12, Software reset CPU*/
    case 13 : sprintf (buff, "RTCWDT_CPU_RESET");break;      /**<13, RTC Watch dog Reset CPU*/
    case 14 : sprintf (buff, "EXT_CPU_RESET");break;         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : sprintf (buff, "RTCWDT_BROWN_OUT_RESET");break;/**<15, Reset when the vdd voltage is not stable*/
    case 16 : sprintf (buff, "RTCWDT_RTC_RESET");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : sprintf (buff, "NO_MEAN");
  }
  log_report(LOG_REPORT_INIT, buff);
  REASON_RESET_PRINTF(buff);
}

void verbose_print_reset_reason(RESET_REASON reason)
{
  char buff[60];
  switch ( reason)
  {
    case 1  : sprintf (buff, "Vbat power on reset");break;
    case 3  : sprintf (buff, "Software reset digital core");break;
    case 4  : sprintf (buff, "Legacy watch dog reset digital core");break;
    case 5  : sprintf (buff, "Deep Sleep reset digital core");break;
    case 6  : sprintf (buff, "Reset by SLC module, reset digital core");break;
    case 7  : sprintf (buff, "Timer Group0 Watch dog reset digital core");break;
    case 8  : sprintf (buff, "Timer Group1 Watch dog reset digital core");break;
    case 9  : sprintf (buff, "RTC Watch dog Reset digital core");break;
    case 10 : sprintf (buff, "Instrusion tested to reset CPU");break;
    case 11 : sprintf (buff, "Time Group reset CPU");break;
    case 12 : sprintf (buff, "Software reset CPU");break;
    case 13 : sprintf (buff, "RTC Watch dog Reset CPU");break;
    case 14 : sprintf (buff, "for APP CPU, reseted by PRO CPU");break;
    case 15 : sprintf (buff, "Reset when the vdd voltage is not stable");break;
    case 16 : sprintf (buff, "RTC Watch dog reset digital core and rtc module");break;
    default : sprintf (buff, "NO_MEAN");
  }
  log_report(LOG_REPORT_INIT, buff);
  REASON_RESET_PRINTF("\r\n%s\r\n", buff);
}

void reason_reset_log(void)
{
    REASON_RESET_PRINTF("\r\nCPU0 reset reason:");
    log_report(LOG_REPORT_INIT, (char*)"CPU0 reset reason:");
    print_reset_reason(rtc_get_reset_reason(0));
    verbose_print_reset_reason(rtc_get_reset_reason(0));

    REASON_RESET_PRINTF("\r\nCPU1 reset reason:");
    log_report(LOG_REPORT_INIT, (char*)"CPU1 reset reason:");
    print_reset_reason(rtc_get_reset_reason(1));
    verbose_print_reset_reason(rtc_get_reset_reason(1));
}