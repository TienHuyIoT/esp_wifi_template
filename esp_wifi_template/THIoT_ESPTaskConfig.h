#ifndef	__ESP_TASK_CONFIG_H
#define __ESP_TASK_CONFIG_H

#include <Arduino.h>

#define TASK_LOOP_POLL_STACK            (1024U * 5)

#define TASK_DELETE_DELAY_MS            10U
#define TASK_CREATE_DELAY_MS            200U
#define TASK_DELETE_USING_TICKER        0

// 1. Task monitor config, static task but it is not used in this project
#define TASK_MONITOR_RUNNING_CORE       -1
#define TASK_MONITOR_STACK              (1024U * 2)
#define TASK_MONITOR_PRIORITY           3
#define TASK_MONITOR_NAME               "Monitor"

// 2. Task i2c poll config
#define TASK_I2C_DATA_HANDLE_TASK       1
#define TASK_I2C_STATIC                 1

#define TASK_I2C_POLL_RUNNING_CORE      0
#define TASK_I2C_POLL_STACK             (1024U * 2 + 512)
#define TASK_I2C_POLL_PRIORITY          3
#define TASK_I2C_POLL_NAME              "i2c_poll"
#if (TASK_I2C_STATIC == 1)
extern StackType_t  _i2c_poll_xStack[TASK_I2C_POLL_STACK];
extern StaticTask_t _i2c_poll_xTaskBuffer;
#endif
extern TaskHandle_t _i2c_poll_task_handle;

#if (TASK_I2C_DATA_HANDLE_TASK == 1)
// 3. I2C data handle config
#define TASK_I2C_DATA_HANDLE_RUNNING_CORE   1
#define TASK_I2C_DATA_HANDLE_STACK          (1024U * 4 + 512)
#define TASK_I2C_DATA_HANDLE_PRIORITY       3
#define TASK_I2C_DATA_HANDLE_NAME           "i2c_data"
#if (TASK_I2C_STATIC == 1)
extern StackType_t  _i2c_data_xStack[TASK_I2C_DATA_HANDLE_STACK];
extern StaticTask_t _i2c_data_xTaskBuffer;
#endif
extern TaskHandle_t _i2c_data_task_handle;
extern EventGroupHandle_t _i2c_data_task_events_group;
#define I2C_PUMP_DATA_BIT_0                 (1U << 0) // Bit for pump data task
#define I2C_EVENT_DATA_BIT_1                (1U << 1) // Bit for event data task
#define PARAM_SETTING_DATA_BIT_5            (1U << 5) // Bit for parameter setting task
#define HTTP1_EVENT_PUMP_LOG_BIT_8          (1U << 8) // Bit for HTTP event pump log task
#define HTTP1_STATION_INFO_BIT_9            (1U << 9) // Bit for HTTP station info task
#define HTTP2_EVENT_PUMP_LOG_BIT_10         (1U << 10) // Bit for HTTP event pump log task
#define HTTP2_STATION_INFO_BIT_11           (1U << 11) // Bit for HTTP station info task
#define HTTP1_PROBE_INFO_BIT_12             (1U << 12) // Bit for HTTP probe info task
#define HTTP1_CARD_INFO_BIT_13              (1U << 13) // Bit for HTTP probe info task
#define HTTP2_PROBE_INFO_BIT_14             (1U << 14) // Bit for HTTP probe info task
#define HTTP2_CARD_INFO_BIT_15              (1U << 15) // Bit for HTTP probe info task
#define I2C_DATA_TASK_BIT_MASK              (I2C_PUMP_DATA_BIT_0        | \
                                            I2C_EVENT_DATA_BIT_1        | \
                                            PARAM_SETTING_DATA_BIT_5    | \
                                            HTTP1_EVENT_PUMP_LOG_BIT_8  | \
                                            HTTP1_STATION_INFO_BIT_9    | \
                                            HTTP2_EVENT_PUMP_LOG_BIT_10 | \
                                            HTTP2_STATION_INFO_BIT_11   | \
                                            HTTP1_PROBE_INFO_BIT_12     | \
                                            HTTP1_CARD_INFO_BIT_13      | \
                                            HTTP2_PROBE_INFO_BIT_14     | \
                                            HTTP2_CARD_INFO_BIT_15      )

#else // #if (TASK_I2C_DATA_HANDLE_TASK == 1)

// 3. Task pump log config
#define TASK_PUMP_LOG_RUNNING_CORE      1
#define TASK_PUMP_LOG_STACK             (1024U * 5)
#define TASK_PUMP_LOG_PRIORITY          3
#define TASK_PUMP_LOG_NAME              "pump_log"
#if (TASK_I2C_STATIC == 1)
extern StackType_t  _i2c_pumpLog_xStack[TASK_PUMP_LOG_STACK];
extern StaticTask_t _i2c_pumpLog_xTaskBuffer;
#endif
extern TaskHandle_t _i2c_pumpLog_task_handle;

// 4. Task i2c event config
#define TASK_I2C_EVENT_RUNNING_CORE     -1
#define TASK_I2C_EVENT_STACK            (1024U * 5)
#define TASK_I2C_EVENT_PRIORITY         3
#define TASK_I2C_EVENT_NAME             "i2c_event"
#if (TASK_I2C_STATIC == 1)
extern StackType_t  _i2c_eventData_xStack[TASK_I2C_EVENT_STACK];
extern StaticTask_t _i2c_eventData_xTaskBuffer;
#endif
extern TaskHandle_t _i2c_eventData_task_handle;
#endif // # if (TASK_I2C_DATA_HANDLE_TASK == 1)

// 5. Task i2c frame config, dynamic task
#define TASK_I2C_FRAME_STATIC           0
#define TASK_I2C_FRAME_RUNNING_CORE     -1
#define TASK_I2C_FRAME_STACK            (1024U * 3)
#define TASK_I2C_FRAME_PRIORITY         3
#define TASK_I2C_FRAME_NAME             "i2c_frame"
#if (TASK_I2C_FRAME_STATIC == 1)
extern StackType_t  _i2c_frame_xStack[TASK_I2C_FRAME_STACK];
extern StaticTask_t _i2c_frame_xTaskBuffer;
#endif
extern TaskHandle_t _i2c_frame_task_handle;

// 6. Task probe tank config, dynamic task by config probeEnableGet()
#define TASK_PROBE_TANK_STATIC          0
#define TASK_PROBE_TANK_RUNNING_CORE    1
#define TASK_PROBE_TANK_STACK           (1024U * 2)
#define TASK_PROBE_TANK_PRIORITY        3
#define TASK_PROBE_TANK_NAME            "probe_tank"
#if (TASK_PROBE_TANK_STATIC == 1)
extern StackType_t  _probe_xStack[TASK_PROBE_TANK_STACK];
extern StaticTask_t _probe_xTaskBuffer;
#endif
extern TaskHandle_t _probe_task_handle;

// 7. Task http log config, dynamic task
#define TASK_HTTP_LOG_STATIC            1
#define TASK_HTTP_LOG_RUNNING_CORE      0
#define TASK_HTTP_LOG_STACK             (1024U * 4 + 512)
#define TASK_HTTP_LOG_PRIORITY          3
#define TASK_HTTP_LOG_NAME              "http_log"
#if (TASK_HTTP_LOG_STATIC == 1)
extern StackType_t  *_http_log_xStack;
extern StaticTask_t *_http_log_xTaskBuffer;
#endif
extern TaskHandle_t _http_log_task_handle;

// 8. Task printer config, dynamic task
#define TASK_PRINTER_STATIC             0
#define TASK_PRINTER_RUNNING_CORE       1
#define TASK_PRINTER_STACK              (1024U * 2 + 512)
#define TASK_PRINTER_PRIORITY           3
#define TASK_PRINTER_NAME               "printer"
#if (TASK_PRINTER_STATIC == 1)
extern StackType_t  _printer_log_xStack[TASK_PRINTER_STACK];
extern StaticTask_t _printer_log_xTaskBuffer;
#endif
extern TaskHandle_t _printer_log_task_handle;

// 9. Task editor chunk config - 2026-10-15: UNUSED.  The ?list= directory walk no longer runs
// on its own task (the response filler does it on async_tcp, bounded per call), so nothing
// creates "editor_chunk" any more.  Kept for reference / a possible revert.
#define TASK_EDITOR_CHUNK_STATIC        0
#define TASK_EDITOR_CHUNK_RUNNING_CORE  0
#define TASK_EDITOR_CHUNK_STACK         (1024U * 4)
#define TASK_EDITOR_CHUNK_PRIORITY      3
#define TASK_EDITOR_CHUNK_NAME          "editor_chunk"

// 10. Task trace log config
#define TASK_TRACE_LOG_STATIC           1
#define TASK_TRACE_LOG_RUNNING_CORE     1
#define TASK_TRACE_LOG_STACK            (1024U * 3 + 512)
#define TASK_TRACE_LOG_PRIORITY         3
#define TASK_TRACE_LOG_NAME             "log_trace"
#if (TASK_TRACE_LOG_STATIC == 1)
extern StackType_t  *_log_trace_xStack;
extern StaticTask_t *_log_trace_xTaskBuffer;
#endif
extern TaskHandle_t _log_trace_task_handle;

// 11. Task ping service config, dynamic task
#define TASK_PING_SERVICE_STATIC        1
#define TASK_PING_SERVICE_RUNNING_CORE  1
#define TASK_PING_SERVICE_STACK         (1024U * 2 + 512)
#define TASK_PING_SERVICE_PRIORITY      3
#define TASK_PING_SERVICE_NAME          "ping_service"
#if (TASK_PING_SERVICE_STATIC == 1)
extern StackType_t  *_ping_service_xStack;
extern StaticTask_t *_ping_service_xTaskBuffer;
#endif
extern TaskHandle_t _ping_service_task_handle;

// 12. Task web socket config, dynamic task
#define TASK_WEB_SOCKET_STATIC          1
#define TASK_WEB_SOCKET_RUNNING_CORE    1
#define TASK_WEB_SOCKET_STACK           (1024U * 5)
#define TASK_WEB_SOCKET_PRIORITY        3
#define TASK_WEB_SOCKET_NAME            "web_socket"
#if (TASK_WEB_SOCKET_STATIC == 1)
extern StackType_t  *_web_socket_xStack;
extern StaticTask_t *_web_socket_xTaskBuffer;
#endif
extern TaskHandle_t _web_socket_task_handle;

// 13. Task console config, dynamic task
#define TASK_CONSOLE_TRACE_STATIC       1
#define TASK_CONSOLE_TRACE_RUNNING_CORE 1
#define TASK_CONSOLE_TRACE_STACK        (1024U * 4 + 512)
#define TASK_CONSOLE_TRACE_PRIORITY     3
#define TASK_CONSOLE_TRACE_NAME         "console_trace"
#if (TASK_CONSOLE_TRACE_STATIC == 1)
extern StackType_t  *_console_trace_xStack;
extern StaticTask_t *_console_trace_xTaskBuffer;
#endif
extern TaskHandle_t _console_trace_task_handle;

// 14. Task Async TCP
extern TaskHandle_t _async_service_task_handle;
// 15. Task LwIP
extern TaskHandle_t _lwIP_TaskHandle;
// 16. Task Arduino Loop
extern TaskHandle_t loopTaskHandle;

// 17. Task total log, dynamic task
#define TASK_TOTAL_LOG_STATIC             1
#define TASK_TOTAL_LOG_RUNNING_CORE       1
#define TASK_TOTAL_LOG_STACK              (1024U * 4 + 512)
#define TASK_TOTAL_LOG_PRIORITY           3
#define TASK_TOTAL_LOG_NAME               "total_log"

#endif // __ESP_TASK_CONFIG_H
