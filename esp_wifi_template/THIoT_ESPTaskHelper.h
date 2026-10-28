#ifndef __ESP_TASK_HELPER_H
#define __ESP_TASK_HELPER_H

#include <Arduino.h>
#include <functional>
#include "THIoT_ESPTaskConfig.h"
#include "THIoT_PFTicker.h"

/* ============================================================================
 * Deferred task delete - option (b): one-shot "delete done" semaphore
 * ============================================================================
 *
 * Every recreated worker task in this project follows the same life cycle:
 *
 *   1. the task body leaves its receive loop and registers a one-shot ticker,
 *      then PARKS on vTaskDelay(portMAX_DELAY) - it does NOT self delete;
 *   2. the ticker fires TASK_DELETE_DELAY_MS later and, under the task mutex,
 *      calls vTaskDelete(handle) (external delete), clears the handle and frees
 *      the static TCB/stack buffers;
 *   3. the next producer calls the _start_xxx_task() helper, which recreates the
 *      task.
 *
 * Step 1/2 order matters. With a SELF delete the TCB is queued in
 * xTasksWaitingTermination and only the idle task unlinks it; freeing the buffer
 * from the ticker then races that idle pass and faults the device
 * (LoadProhibited in vTaskSwitchContext). The external delete above removes that
 * window, which is why the body only parks.
 *
 * Step 3 must not run before step 2. The ticker *signals* a binary semaphore and
 * the helper *waits* on it - without a timeout, because a signal is always
 * produced whenever the handle is still set (that is exactly what arms the
 * ticker). Freeing the wait of a timeout is only safe together with the
 * alloc-failure fallback below; see task_alloc_delete_ticker().
 *
 * Usage:
 *
 *   // 1. one semaphore per task slot, created next to the task mutex
 *   static SemaphoreHandle_t _xxx_delete_sem = nullptr;
 *   ...
 *   if (!_xxx_task_mutex) {
 *       _xxx_task_mutex = xSemaphoreCreateMutex();
 *       if (!_xxx_task_mutex) { return false; }
 *       _xxx_delete_sem = task_delete_sem_create();
 *   }
 *
 *   // 2. the clearer (ticker callback) signals after it cleared the handle
 *   xSemaphoreGive(_xxx_task_mutex);
 *   task_delete_sem_signal(_xxx_delete_sem);
 *
 *   // 3. the waiter (recreate path)
 *   xSemaphoreTake(_xxx_task_mutex, portMAX_DELAY);
 *   if (_xxx_task_handle != NULL) {
 *       task_wait_pending_delete(_xxx_task_mutex, _xxx_delete_sem);
 *   }
 *   if (_xxx_task_handle == NULL) { ...create... }
 *   xSemaphoreGive(_xxx_task_mutex);
 *
 * NOTE (same-task case, e.g. THIoT_ESPPingService.cpp): ticker callbacks run in
 * loopTask (THIoT_PFTicker.cpp -> ticker_schedule_handler(), whose only caller is
 * loop() in web485_v3.ino). If the recreate path is *also* driven by a ticker
 * callback, the clearer cannot run while the waiter blocks, so the wait would
 * never complete. Such a module must NOT use this wait. It is safe because the
 * intervening loops (interval >= 1 s) already let the ticker fire.
 */

/* Callback type of the deferred delete ticker. Same type as fp_callback in
 * THIoT_PFTicker.h, declared here to keep this header dependency-free. */
using task_ticker_cb_t = std::function<void(void *)>;

/**
 * @brief Create the binary "delete done" semaphore of one task slot.
 *        Returns nullptr on allocation failure (the wait then degrades to the cap).
 */
static inline SemaphoreHandle_t task_delete_sem_create()
{
    return xSemaphoreCreateBinary();
}

/**
 * @brief Drop one pending (stale) signal before a wait, so a signal released in a
 *        previous delete cycle cannot make the next wait return early.
 */
static inline void task_delete_sem_reset(SemaphoreHandle_t sem)
{
    if (sem) {
        xSemaphoreTake(sem, 0);
    }
}

/**
 * @brief Signal "delete done": the clearer has cleared the handle and freed the
 *        buffers. Called by the one-shot ticker callback.
 */
static inline void task_delete_sem_signal(SemaphoreHandle_t sem)
{
    if (sem) {
        xSemaphoreGive(sem);
    }
}

/**
 * @brief Arm the one-shot "delete" ticker of a task slot.
 *
 * Wraps allocate_ticker_once_ms() and CHECKS the result. When the ticker cannot
 * be allocated (registry full, malloc failure) the deferred cleanup would never
 * run: nobody would clear the handle, so the next recreate would wait for ever.
 * The caller must therefore run task_delete_fallback() when this returns false.
 *
 * @param interval_ms delay before the cleanup runs.
 * @param cb          cleanup callback (clears the handle, frees, then signals).
 * @param arg         argument passed to cb (usually nullptr).
 * @return true when the ticker is armed, false when allocation failed.
 */
static inline bool task_alloc_delete_ticker(uint32_t interval_ms, task_ticker_cb_t cb, void *arg = nullptr)
{
    return allocate_ticker_once_ms(interval_ms, cb, arg) != nullptr;
}

/**
 * @brief Fallback used when task_alloc_delete_ticker() failed.
 *
 * No ticker exists, so the handle is cleared here and the body then self deletes.
 * The heap/static buffers are deliberately KEPT so the next recreate reuses them:
 * releasing only the handle avoids both a leak and the free of memory this task is
 * still executing on.
 *
 * The mutex is taken with a timeout on purpose. The recreate path takes the same
 * mutex and then blocks in task_wait_pending_delete(), so an endless take here
 * could deadlock against that waiter. When the take times out the caller must NOT
 * create a task - a concurrent starter already owns the slot.
 *
 * @return true  the handle was cleared; the slot can be recreated,
 *         false the mutex is busy (another starter owns the slot).
 */
static inline bool task_delete_fallback(SemaphoreHandle_t mutex, TaskHandle_t *handle)
{
    if (!mutex) {
        return false;
    }
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(TASK_CREATE_DELAY_MS)) != pdTRUE) {
        return false;
    }
    *handle = nullptr;
    xSemaphoreGive(mutex);
    return true;
}

/**
 * @brief Block until the clearer signals "delete done". Call it with `mutex`
 *        RELEASED.
 *
 * The wait is endless by design: a signal is always produced whenever the handle
 * is still set, because that is precisely what arms the delete ticker (see
 * task_alloc_delete_ticker()). A stale signal of a previous cycle is dropped by
 * the reset inside task_wait_pending_delete().
 *
 * @param sem "delete done" semaphore of the task slot (nullptr is tolerated).
 */
static inline void task_wait_delete_done(SemaphoreHandle_t sem)
{
    if (!sem) {
        return;
    }
    xSemaphoreTake(sem, portMAX_DELAY);
}

/**
 * @brief Waiter side of the deferred delete. MUST be called while holding `mutex`
 *        and with the handle still non-null.
 *
 * Drops a stale "delete done" signal, releases `mutex` (so the clearer can run),
 * blocks until the clearer signals, then re-acquires `mutex`. On return the handle
 * is null and the slot can be recreated.
 */
static inline void task_wait_pending_delete(SemaphoreHandle_t mutex, SemaphoreHandle_t sem)
{
    task_delete_sem_reset(sem);   /* drop a stale signal of a previous cycle */
    xSemaphoreGive(mutex);
    task_wait_delete_done(sem);   /* endless: the clearer always signals */
    xSemaphoreTake(mutex, portMAX_DELAY);
}

#endif // __ESP_TASK_HELPER_H
