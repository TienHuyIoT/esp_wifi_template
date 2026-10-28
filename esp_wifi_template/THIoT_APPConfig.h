#ifndef	__APP_CONFIG_H
#define __APP_CONFIG_H

#include <sdkconfig.h>
#include <esp_idf_version.h>

/**
 * @brief development phase
 * 0: develop or update
 * 1. Release
*/
#define FIRMWARE_RELEASE_ACTIVATED      1

/**
 * @brief Console trace debug. Streams: serial + file (FS) + WebSocket.
 * 0: Disable
 * 1. Enable
*/
#define CONSOLE_TRACE_TASK_ENABLE       1
#define SERIAL_TRACE_TASK_ENABLE        1

/**
 * @brief Attach free heap (free RAM) to every SERIAL log line.
 *        Useful to detect memory leaks: the free heap is printed as
 *        a "H<heap> | " prefix in front of each log message.
 * 0: Disable
 * 1: Enable
*/
#define SERIAL_HEAP_TRACE_ENABLE        0

/**
 * @brief Attach a wall-clock timestamp (hh:mm:ss) to every SERIAL log line,
 *        printed before the free heap. Requires the system time to be set
 *        (RTC or SNTP; before that the time is the epoch and shows 00:00:00).
 * 0: Disable
 * 1: Enable
*/
#define SERIAL_TIME_TRACE_ENABLE        0

#if (CONSOLE_TRACE_TASK_ENABLE == 1)
#define CONSOLE_TRACE_ASYNC_TCP         1
#define CONSOLE_TRACE_ASYNC_WEBSERVER   1
#define CONSOLE_TRACE_ASYNC_HTTP        1
#else
#define CONSOLE_TRACE_ASYNC_TCP         0
#define CONSOLE_TRACE_ASYNC_WEBSERVER   0
#define CONSOLE_TRACE_ASYNC_HTTP        0
#endif

/**
 * @brief Core dump record
 * 0: Disable
 * 1: Enable
*/
#if (ESP_IDF_VERSION_MAJOR >= 4)
#define CORE_DUMP_TRACE_RECORD          1 // 1. only for debug
#else
#define CORE_DUMP_TRACE_RECORD          0
#endif

/**
 * @brief OTA confirmation
 * 0: Disable
 * 1: Enable
*/
#define OTA_CONFIRM_FIRMWARE_VALIDATION 1

/**
 * @brief OTA rollback
 * 0: Disable, it means that the firmware is valid after OTA update
 * 1: Enable, it means that the firmware is invalid after OTA update, and the firmware has to be confirmed valid by the application
*/
#define FIRMWARE_OTA_ROLLBACK_ENABLE    0

/** @brief configure kind of time zone
 * 0: TzTime type
 * 1: gtmOffset (recommend for accuracy)
*/
#define TIME_ZONE_TYPE_CFG              1

/** @brief manual synchronous time to main.
 * 1: Enable
 * 0: Disable
 * Note: sntp service shall be automatic trigger sync time with interval 1hour
*/
#define INTERVAL_SYNC_TIME_TO_MAIN      0
/** @brief interval synchronous time
 * This depends on the macro above
*/
#define RTC_INTERVAL_SYNCHRONOUS_NUM_S  (60 * 30) // 30 minutes

/** @brief Mutex memory PFTicker
 *  NOTE: THIoT_PFTicker.cpp (ported from web485_v3) always uses the mutex mode:
 *        ticker_t carries the per-ticker semaphore and the scheduler takes it,
 *        so this has to stay 1.
*/
#define MUTEX_PLATFORM_TICKER           1

/**
 * @brief reset esp after timeout if network disconnect
*/
#define NETWORK_CONNECTION_TIMEOUT_RESET 1

/* ESP32, ESP8266 is little endian, STM8 is big endian, AVR is little endian */
#define LITTLE_ENDIAN_TYPE              1

/**
 * @brief Type of ESPAsyncWebServer used
 * 0. https://github.com/me-no-dev/ESPAsyncWebServer
 * 1. https://github.com/mathieucarbou/ESPAsyncWebServer
 */
#define ASYNC_WEB_SERVER_MIDDLEWARE_USED    1
#define ASYNC_WEB_SOCKET_SHARED_BUFFER      1

/**@brief folder used by the application configuration files*/
#define APP_FOLDER_CFG_PATH             "/CONFIG"

/**
 * @brief Replace to xRingBuff
 * 0: Disable
 * 1: Enable
*/
#define MESSAGE_QUEUE_REPLACE_X_RING_BUFFER 1

/**
 * @brief TCP client debug
 * 0: Disable
 * 1: Enable
*/
#define TCP_CLIENT_TRACE_START_ON_WIFI_EVENT  0

/**
 * Clean up memory
*/
#define CLEAN_UP_NAND_MEM   1
#define CLEAN_UP_SD_CARD    0

#define WEB_SERVER_REDIRECT_ENABLE      0

/**
 * @brief Calendar schedule runtime
 * 0: Disable
 * 1: Enable
*/
#define CALENDAR_SCHEDULE_ENABLE            1

#endif // __APP_CONFIG_H
