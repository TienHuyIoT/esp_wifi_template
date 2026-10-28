#ifndef	__ESP_TASK_MONITOR_H
#define __ESP_TASK_MONITOR_H

#include <Arduino.h>
#include <vector>
#include "THIoT_PFTicker.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_ESPTaskConfig.h"

#define MONITOR_TASK_INTERVAL_MS            (5 * 60000U)  // 5 minutes
#define MONITOR_TASK_HANDLER                0 // Enable task monitor handler

extern ESPLogTrace MONITOR_TASK_FS_RECORD;
void task_hight_water_mark_console(TaskHandle_t taskMonitor);

class THIoT_ESPTaskMonitor
{
public:
    THIoT_ESPTaskMonitor();
    ~THIoT_ESPTaskMonitor();
    void begin(size_t interval = MONITOR_TASK_INTERVAL_MS);
    void addTask(TaskHandle_t *taskHandle);
    void removeTask(TaskHandle_t *taskHandle);
    void clear();
    void setEnabled(bool enabled);
    void log();
    bool isEnabled() const { return _enabled; }

private:
    ticker_function_handle_t _monitorTicker;
    size_t _interval;
    bool _enabled;
    std::vector<TaskHandle_t *> _taskHandlers;
#if (MONITOR_TASK_HANDLER == 1)
    TaskHandle_t _monitor_task_handle;
    static void _monitor_task(void *pvParameters);
    void monitorLoop();
#endif
};

extern THIoT_ESPTaskMonitor ESPTaskMonitor;

#endif // __ESP_TASK_MONITOR_H