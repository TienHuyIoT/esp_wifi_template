#ifndef __SERIAL_TRACE_H
#define __SERIAL_TRACE_H

#include <Arduino.h>
#include <time.h>
#include "THIoT_APPConfig.h"

#define STRINGIFY(x)        #x
#define TOSTRING(x)         STRINGIFY(x)
#define SERIAL_PORT         Serial
#define SERIAL_BAUDRATE     921600

/* ---------------------------------------------------------------------------
 * Optional SERIAL log prefix (THIoT_APPConfig.h):
 *   SERIAL_TIME_TRACE_ENABLE -> wall-clock "hh:mm:ss " before the heap
 *   SERIAL_HEAP_TRACE_ENABLE -> free heap "H<heap> | " before the message
 * Example with both on:
 *   14:03:07 H213456 | I WIFI: got IP address
 * The prefix and the message are formatted into one buffer and written with a
 * SINGLE write(), so two tasks logging at the same time cannot interleave.
 * A line longer than the stack buffer is retried on the heap (same policy as
 * Print::vprintf in the Arduino core), so no log is truncated. The variadic
 * template forwards the arguments directly, which keeps them type-safe (no
 * %u/%lu mismatch) and lets the format be evaluated twice without a va_list.
 * When both features are off the macro maps straight to printf_P, i.e. the
 * original behaviour.
 * ------------------------------------------------------------------------- */
#if (SERIAL_TIME_TRACE_ENABLE == 1) || (SERIAL_HEAP_TRACE_ENABLE == 1)
/* Build the enabled parts of the prefix into [dst]. Returns the length. */
inline size_t serialPrefixBuild(char *dst, size_t size)
{
    size_t off = 0;
#if (SERIAL_TIME_TRACE_ENABLE == 1)
    {
        struct tm timeInfo;
        time_t now = time(nullptr);
        if (localtime_r(&now, &timeInfo) != nullptr) {
            int n = snprintf(dst, size, "%02d:%02d:%02d ",
                             timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
            if (n > 0) {
                off = ((size_t)n < size) ? (size_t)n : (size - 1);
            }
        }
    }
#endif
#if (SERIAL_HEAP_TRACE_ENABLE == 1)
    {
        if (off < size) {
            int n = snprintf(dst + off, size - off, "H%06u | ", (unsigned)ESP.getFreeHeap());
            if (n > 0) {
                off += ((size_t)n < (size - off)) ? (size_t)n : (size - off - 1);
            }
        }
    }
#endif
    return off;
}

template <typename... Args>
inline void serialPrintfPrefixed(const char *format, Args... args)
{
    char temp[192];
    size_t off = serialPrefixBuild(temp, sizeof(temp));

    int msgLen = snprintf(temp + off, sizeof(temp) - off, format, args...);
    if (msgLen < 0) {
        return;
    }
    size_t total = off + (size_t)msgLen;
    if (total < sizeof(temp)) {
        SERIAL_PORT.write((const uint8_t *)temp, total);
        return;
    }

    /* Long line: format again into an exact-size heap buffer. If the allocation
     * fails the (truncated) stack copy is written instead, so the log is never
     * dropped entirely. */
    char *buffer = (char *)malloc(total + 1);
    if (buffer == nullptr) {
        SERIAL_PORT.write((const uint8_t *)temp, sizeof(temp) - 1);
        return;
    }
    memcpy(buffer, temp, off);
    snprintf(buffer + off, total - off + 1, format, args...);
    SERIAL_PORT.write((const uint8_t *)buffer, total);
    free(buffer);
}
#define SERIAL_HEAP_PREFIX()                ::serialPrintfPrefixed
#else
#define SERIAL_HEAP_PREFIX()                SERIAL_PORT.printf_P
#endif // #if (SERIAL_TIME_TRACE_ENABLE == 1) || (SERIAL_HEAP_TRACE_ENABLE == 1)

// Simple debug macro
/* TAG is a %s ARGUMENT instead of being pasted into the format string, so both
 * fields can be padded/truncated to a fixed width and the log stays
 * column-aligned. Widen %-18.18s if a Tag is longer than 18 characters.
 * __LINE__ is passed with %04u so the line column is a FIXED 4 digits
 * (e.g. "line 0349:"). The %04u and the (unsigned)__LINE__ argument MUST stay
 * paired: dropping the %04u would shift every later argument and make snprintf
 * read the line number as a %s pointer (crash: LoadProhibited, EXCVADDR = line). */
#define SERIAL_CONSOLE(f_, ...)         SERIAL_PORT.printf_P(__VA_ARGS__)
#define SERIAL_CONSOLE_I(TAG, f_, ...)  SERIAL_HEAP_PREFIX()(PSTR("I " "%-18.18s" "| %-20.20s" "| line %04u: " f_ "\r\n"),  TAG, __func__, (unsigned)__LINE__, ##__VA_ARGS__)
#define SERIAL_CONSOLE_E(TAG, f_, ...)  SERIAL_HEAP_PREFIX()(PSTR("E " "%-18.18s" "| %-20.20s" "| line %04u: " f_ "\r\n"),  TAG, __func__, (unsigned)__LINE__, ##__VA_ARGS__)

#if (SERIAL_TRACE_TASK_ENABLE == 1)
#define gSerialDebugLevel   4   /* 0: OFF; 4:ON */
#else
#define gSerialDebugLevel   0   /* 0: OFF; 4:ON */
#endif

#if (0)
#include <iostream>
#include <string_view>
#include <source_location>

void log(const std::string_view message,
         const std::source_location location =
               std::source_location::current())
{
    std::cout << "file: "
              << location.file_name() << "("
              << location.line() << ":"
              << location.column() << ") `"
              << location.function_name() << "`: "
              << message << '\n';
}
#endif

#if (gSerialDebugLevel > 0)
#define SERIAL_FLUSH() (SERIAL_PORT.flush())
#define SERIAL_PRINTF(f_, ...) SERIAL_HEAP_PREFIX()(PSTR(f_), ##__VA_ARGS__)
#define SERIAL_PRINTFLN(f_, ...) SERIAL_HEAP_PREFIX()(PSTR(f_ "\r\n"), ##__VA_ARGS__)
#define SERIAL_TAG_PRINTF(tag, f_, ...)  SERIAL_PORT.printf_P(PSTR(tag f_), ##__VA_ARGS__)
#define SERIAL_FUNCTION_PRINTFLF(f_, ...)  SERIAL_PORT.printf_P(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)

#define SERIAL_LOGE(...) do{if(gSerialDebugLevel > 0) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)
#define SERIAL_LOGW(...) do{if(gSerialDebugLevel > 1) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)
#define SERIAL_LOGI(...) do{if(gSerialDebugLevel > 2) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)
#define SERIAL_LOGD(...) do{if(gSerialDebugLevel > 3) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)
#define SERIAL_LOGV(...) do{if(gSerialDebugLevel > 4) {SERIAL_PRINTF(__VA_ARGS__);}}while(0)

/* The message format is a separate parameter (f_) so the whole line -
 * "I " %-18.18s "| " %-20.20s "| line %04u: " f_ - is one printf_P format
 * string. x (Tag) and __func__ are data ARGUMENTS, padded/cut to a fixed width
 * so Tag and function name line up in columns.
 * KEEP %04u PAIRED with (unsigned)__LINE__ in the argument list: the line
 * column is a fixed 4 digits, but a missing %04u would shift every later
 * argument and crash in snprintf (see the SERIAL_CONSOLE_I note above). */
#define SERIAL_TAG_LOGE(x, f_, ...) do{if(gSerialDebugLevel > 0) {SERIAL_PRINTFLN("E " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)
#define SERIAL_TAG_LOGW(x, f_, ...) do{if(gSerialDebugLevel > 1) {SERIAL_PRINTFLN("W " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)
#define SERIAL_TAG_LOGI(x, f_, ...) do{if(gSerialDebugLevel > 2) {SERIAL_PRINTFLN("I " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)
#define SERIAL_TAG_LOGD(x, f_, ...) do{if(gSerialDebugLevel > 3) {SERIAL_PRINTFLN("D " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)
#define SERIAL_TAG_LOGV(x, f_, ...) do{if(gSerialDebugLevel > 4) {SERIAL_PRINTFLN("V " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)

#define SERIAL_FUNCTION_IN(...) do{SERIAL_FUNCTION_PRINTFLF("\r\n\r\nFUNCTION [%s] IN " __VA_ARGS__);}while(0)
#define SERIAL_FUNCTION_OUT(...) do{SERIAL_FUNCTION_PRINTFLF("\r\nFUNCTION [%s] OUT " __VA_ARGS__);}while(0)
#define SERIAL_FUNCTION_LOGI(...) do{SERIAL_FUNCTION_PRINTFLF("\r\n\r\nI: FUNCTION [%s] " __VA_ARGS__);}while(0)
#define SERIAL_FUNCTION_TAG_LOGI(x, ...) do{SERIAL_FUNCTION_PRINTFLF("I " x ": FUNCTION [%s] " __VA_ARGS__);}while(0)
#else
#define SERIAL_FLUSH()
#define SERIAL_PRINTF(f_, ...)
#define SERIAL_PRINTFLN(f_, ...)
#define SERIAL_TAG_PRINTF(tag, f_, ...)
#define SERIAL_FUNCTION_PRINTFLF(f_, ...)

#define SERIAL_LOGE(...)
#define SERIAL_LOGW(...)
#define SERIAL_LOGI(...)
#define SERIAL_LOGD(...)
#define SERIAL_LOGV(...)

#define SERIAL_TAG_LOGE(x, f_, ...)
#define SERIAL_TAG_LOGW(x, f_, ...)
#define SERIAL_TAG_LOGI(x, f_, ...)
#define SERIAL_TAG_LOGD(x, f_, ...)
#define SERIAL_TAG_LOGV(x, f_, ...)

#define SERIAL_FUNCTION_IN(...)
#define SERIAL_FUNCTION_OUT(...)
#define SERIAL_FUNCTION_LOGI(...)
#define SERIAL_FUNCTION_TAG_LOGI(x, ...)
#endif // #if (gSerialDebugLevel > 0)

#if (CONSOLE_TRACE_TASK_ENABLE == 1)
#include "THIoT_PFConsoleTrace.h"
#else
#define CONSOLE_TRACE_SERIAL_STREAM         0
#define CONSOLE_TRACE_FILE_STREAM           0
#define CONSOLE_TRACE_UDP_STREAM            0

#if (gSerialDebugLevel > 0)
#define CONSOLE_PORT                        SERIAL_PORT
#define CONSOLE_FLUSH()                     (CONSOLE_PORT.flush())
#define CONSOLE_PRINTF(f_, ...)             SERIAL_HEAP_PREFIX()(PSTR(f_), ##__VA_ARGS__)
#define CONSOLE_PRINTFLN(f_, ...)           SERIAL_HEAP_PREFIX()(PSTR(f_ "\r\n"), ##__VA_ARGS__)
#define CONSOLE_TAG_PRINTF(tag, f_, ...)    SERIAL_HEAP_PREFIX()(PSTR(tag f_), ##__VA_ARGS__)
#define CONSOLE_FUNCTION_PRINTFLF(f_, ...)  SERIAL_HEAP_PREFIX()(PSTR(f_ "\r\n"), __FUNCTION__, ##__VA_ARGS__)

#define CONSOLE_LOGE(...) do{if(gSerialDebugLevel > 0) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGW(...) do{if(gSerialDebugLevel > 1) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGI(...) do{if(gSerialDebugLevel > 2) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGD(...) do{if(gSerialDebugLevel > 3) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)
#define CONSOLE_LOGV(...) do{if(gSerialDebugLevel > 4) {CONSOLE_PRINTF(__VA_ARGS__);}}while(0)

/* Message format is a separate parameter (f_): do not wrap the prefix in PSTR()
 * here, or PSTR() ends up nested inside the PSTR() of CONSOLE_PRINTFLN.
 * Tag (x) and __func__ are ARGUMENTS, both column-padded like the SERIAL_* set,
 * so serial and console logs use the same layout. The line column is a fixed
 * 4 digits via %04u paired with (unsigned)__LINE__ - keep them together. */
#define CONSOLE_TAG_LOGE(x, f_, ...) do{if(gSerialDebugLevel > 0) {CONSOLE_PRINTFLN("E " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGW(x, f_, ...) do{if(gSerialDebugLevel > 1) {CONSOLE_PRINTFLN("W " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGI(x, f_, ...) do{if(gSerialDebugLevel > 2) {CONSOLE_PRINTFLN("I " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGD(x, f_, ...) do{if(gSerialDebugLevel > 3) {CONSOLE_PRINTFLN("D " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)
#define CONSOLE_TAG_LOGV(x, f_, ...) do{if(gSerialDebugLevel > 4) {CONSOLE_PRINTFLN("V " "%-18.18s" "| %-20.20s" "| line %04u: " f_,  x, __func__, (unsigned)__LINE__, ##__VA_ARGS__);}}while(0)

#define CONSOLE_FUNCTION_IN(...)            do{CONSOLE_FUNCTION_PRINTFLF("\r\n\r\nFUNCTION [%s] IN " __VA_ARGS__);}while(0)
#define CONSOLE_FUNCTION_OUT(...)           do{CONSOLE_FUNCTION_PRINTFLF("\r\nFUNCTION [%s] OUT " __VA_ARGS__);}while(0)
#define CONSOLE_FUNCTION_LOGI(...)          do{CONSOLE_FUNCTION_PRINTFLF("\r\n\r\nI: FUNCTION [%s] " __VA_ARGS__);}while(0)
#define CONSOLE_FUNCTION_TAG_LOGI(x, ...)   do{CONSOLE_FUNCTION_PRINTFLF("I " x ": FUNCTION [%s] " __VA_ARGS__);}while(0)
#else
#define CONSOLE_FLUSH()
#define CONSOLE_PRINTF(f_, ...)
#define CONSOLE_PRINTFLN(f_, ...)
#define CONSOLE_TAG_PRINTF(tag, f_, ...)
#define CONSOLE_FUNCTION_PRINTFLF(f_, ...)

#define CONSOLE_LOGE(...)
#define CONSOLE_LOGW(...)
#define CONSOLE_LOGI(...)
#define CONSOLE_LOGD(...)
#define CONSOLE_LOGV(...)

#define CONSOLE_TAG_LOGE(x, ...)
#define CONSOLE_TAG_LOGW(x, ...)
#define CONSOLE_TAG_LOGI(x, ...)
#define CONSOLE_TAG_LOGD(x, ...)
#define CONSOLE_TAG_LOGV(x, ...)

#define CONSOLE_FUNCTION_IN(...)
#define CONSOLE_FUNCTION_OUT(...)
#define CONSOLE_FUNCTION_LOGI(...)
#define CONSOLE_FUNCTION_TAG_LOGI(x, ...)
#endif // #if (gSerialDebugLevel > 0)
#endif // #if (CONSOLE_TRACE_TASK_ENABLE == 1)

#define TASK_TRACE_TAG_DEBUG_ENABLE         1
#if (TASK_TRACE_TAG_DEBUG_ENABLE == 1)
#define TASK_TRACE_TAG_DEBUG(...)           SERIAL_TAG_LOGI("I TASK TRACE: ", __VA_ARGS__)
#define TASK_TRACE_FUNCTION_TAG_DEBUG(...)  SERIAL_FUNCTION_TAG_LOGI("I TASK TRACE: FUNCTION [%s] ", __VA_ARGS__)
#else
#define CONSOLE_TRACE_TAG_TASK_DEBUG(...)
#define CONSOLE_TRACE_FUNCTION_TAG_TASK_DEBUG(...)
#endif

#endif // __SERIAL_TRACE_H
