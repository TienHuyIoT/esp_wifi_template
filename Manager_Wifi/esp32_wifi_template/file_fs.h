#ifndef	_FILE_FS_H
#define _FILE_FS_H

#define LOG_REPORT_SIZE_BYTE      (1024 * 200)
#define LOG_REPORT_INIT           0
#define LOG_REPORT_SLEEP          1
#define LOG_REPORT_RESET          2
#define LOG_REPORT_WIFI           3
#define LOG_REPORT_TEMP           4
#define LOG_REPORT_OVER_TEMP      5
#define LOG_REPORT_SD             6
#define LOG_REPORT_PATH           "/log_report.csv"

typedef struct
{
  struct
  {
    uint32_t start;
    uint32_t stop;
    int32_t total;
  }cast;
  struct
  {
    uint32_t start;
    uint32_t stop;
    int32_t total;
  }volume;
} pumplog_info_t;

typedef struct
{
  int bill;
  int sn;
  int pump;
  int date;
  int time;
  int cash;
  int volume;
  int cost;
  int tt1;
  int tl1;
  int tlb1;
  int tt2;
  int tl2;
  int tlb2;
} search_log_t;

#define SEARCH_LENGHT_MAX  256
File fs_search_file;
char search_file_buff[SEARCH_LENGHT_MAX + 1]; 
File fs_log_file;

#endif
