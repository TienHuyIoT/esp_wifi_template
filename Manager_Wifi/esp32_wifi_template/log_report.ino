#include <fs.h>
#include "rtc_data_file.h"

void log_report(uint8_t log_id, char *p_log)
{
    File fs_handle;
    if (!NAND_FS_SYSTEM.exists(LOG_REPORT_PATH))
    {
        fs_handle = NAND_FS_SYSTEM.open(LOG_REPORT_PATH, FILE_WRITE);
        fs_handle.println("Date, Time, Heap RAM, Code Reset, Temp, Description");
        fs_handle.close();
    }

    fs_handle = NAND_FS_SYSTEM.open(LOG_REPORT_PATH, FILE_APPEND);

    size_t size = fs_handle.size();
    if (size > LOG_REPORT_SIZE_BYTE)
    {
        fs_handle.close();
        NAND_FS_SYSTEM.remove(LOG_REPORT_PATH);
        fs_handle = NAND_FS_SYSTEM.open(LOG_REPORT_PATH, FILE_WRITE);
        fs_handle.println("Date*, Time*, Heap RAM, Code report, Temp, Description");
    }

    rtc_time_t rtc;
    rtc_get(&rtc);
    fs_handle.printf("%02u/%02u/%04u, %02u:%02u:%02u, %u, %u, %.2f, %s\r\n", 
                        rtc.mday, rtc.mon, rtc.year, rtc.hour, rtc.min, rtc.sec, ESP.getFreeHeap(), log_id, esp32_internal_temp(), p_log);
    fs_handle.close();
}