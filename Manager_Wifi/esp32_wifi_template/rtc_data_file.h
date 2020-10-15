#ifndef	_RTC_DATA_FILE_
#define _RTC_DATA_FILE_

typedef enum {
    RTC_FILE_READ = 0,
    RTC_FILE_WRITE
} rtc_file_cmd_t;

#define RTC_FILE_PATH "/rtc_info_file.txt"

#endif