#include <FS.h>
#include "sd_card.h"
#include "log_report.h"
#include "flatform_rtc.h"
#include "rtc_data_file.h"
#include "app_config.h"

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
#define LOG_REPORT_FS_SYSTEM SD_FS_SYSTEM
#else
#define LOG_REPORT_FS_SYSTEM NAND_FS_SYSTEM
#endif

extern float esp32_internal_temp(void);

void log_report(uint8_t log_id, char *p_log)
{
    File file;
    fs::FS &fs = LOG_REPORT_FS_SYSTEM;
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
    if (!sd_card_status_isOK())
    {
        fs = NAND_FS_SYSTEM;
    }
#endif
    if (!fs.exists(LOG_REPORT_PATH))
    {
        file = fs.open(LOG_REPORT_PATH, FILE_WRITE);
        file.println("Date, Time, Heap RAM, Code Reset, Temp, Description");
        file.close();
    }

    file = fs.open(LOG_REPORT_PATH, FILE_APPEND);

    size_t size = file.size();
    if (size > LOG_REPORT_SIZE_BYTE)
    {
        file.close();
        fs.remove(LOG_REPORT_PATH);
        file = fs.open(LOG_REPORT_PATH, FILE_WRITE);
        file.println("Date*, Time*, Heap RAM, Code report, Temp, Description");
    }

    rtc_time_t rtc;
    rtc_get(&rtc);
    file.printf("%02u/%02u/%04u, %02u:%02u:%02u, %u, %u, %.2f, %s\r\n", 
                        rtc.mday, rtc.mon, rtc.year, rtc.hour, rtc.min, rtc.sec, ESP.getFreeHeap(), log_id, esp32_internal_temp(), p_log);
    file.close();
}