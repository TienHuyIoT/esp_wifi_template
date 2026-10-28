#include "THIoT_ESPMessageList.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFSerialTrace.h"
#include <atomic>
#include <new>

#if (1)
#define MESSAGE_LIST_MUTEX_TAG_LOG(...) FS_TAG_LOGI("[M_LIST_MUTEX]", __VA_ARGS__)
#else
#define MESSAGE_LIST_MUTEX_TAG_LOG(...) SERIAL_TAG_LOGI("[M_LIST_MUTEX]", __VA_ARGS__)
#endif

#define MESSAGE_LIST_MUTEX_CONSOLE(...) //SERIAL_TAG_LOGI("[M_LIST_MUTEX]", __VA_ARGS__)
#define LIST_MUTEX_CONSOLE(...)         //SERIAL_TAG_LOGI("[LIST_MUTEX]", __VA_ARGS__)
#define ELEMENT_LIST_MUTEX_CONSOLE(...) //SERIAL_TAG_LOGI("[E_LIST_MUTEX]", __VA_ARGS__)
#define ELEMENT_LIST_CONSOLE(...)       //SERIAL_TAG_LOGI("[E_LIST]", __VA_ARGS__)

/* F1: every failure record below is written through the FS log, which may target the very
 * file whose row the module just failed to obtain (LOG_TRACE_SD_FILE_PATH).  Emitting the
 * record for that key feeds itself - and a failed enqueue escalates to a soft reset in the
 * log subsystem - so ALL failure logs must pass through this filter, not only the timeout
 * branch.  Success/status records keep using MESSAGE_LIST_MUTEX_TAG_LOG directly. */
static inline bool _mlFailureLoggable(const char *key) {
    return (key == nullptr) || (strcmp(key, LOG_TRACE_SD_FILE_PATH) != 0);
}

#define MESSAGE_LIST_MUTEX_FAIL_LOG(key, ...)                                       \
    do {                                                                            \
        if (_mlFailureLoggable(key)) { MESSAGE_LIST_MUTEX_TAG_LOG(__VA_ARGS__); }   \
    } while (0)

#define _element_mutex_take(x, t)   ((x) && (xSemaphoreTakeRecursive((x), (t)) == pdTRUE))
#define _element_mutex_give(x)      ((x) && (xSemaphoreGiveRecursive(x) == pdTRUE))
#define _element_mutex_delete(x)    do{if ((x) != NULL) {vSemaphoreDelete((x)); (x) = NULL;}}while(0)

#define LINKED_LIST_LIVE_TIME_MS    (60000U * 5) // 5 minutes
#define MESSAGE_LIST_ELEMENTS_LIMIT 64U

/* Counters are shared with other tasks; volatile was never a substitute for atomic (ML10). */
static std::atomic<size_t> l_counter(0);
static std::atomic<size_t> listCounterAllocated(0);

const char *mutexStatusToString(mutex_status_t status) {
    switch (status) {
    case MUTEX_STATUS_OK:               return "OK";
    case MUTEX_STATUS_TABLE_BUSY:       return "table-busy";
    case MUTEX_STATUS_LIST_FULL:        return "list-full";
    case MUTEX_STATUS_ADMISSION_LIMIT:  return "admission-limit";
    case MUTEX_STATUS_TIMEOUT:          return "timeout";
    case MUTEX_STATUS_ALLOC_FAIL:       return "alloc-fail";
    case MUTEX_STATUS_NOT_FOUND:        return "not-found";
    case MUTEX_STATUS_NOT_OWNER:        return "not-owner";
    default:                            return "unknown";
    }
}

/* Tick arithmetic that cannot wrap and understands portMAX_DELAY. */
static inline TickType_t _tickElapsed(TickType_t start) {
    return (TickType_t)(xTaskGetTickCount() - start);
}

static inline bool _tickExpired(TickType_t start, TickType_t wait) {
    if (wait == portMAX_DELAY) { return false; }
    return (_tickElapsed(start) >= wait);
}

static inline TickType_t _tickRemaining(TickType_t start, TickType_t wait) {
    if (wait == portMAX_DELAY) { return portMAX_DELAY; }
    const TickType_t used = _tickElapsed(start);
    return (used >= wait) ? 0 : (TickType_t)(wait - used);
}

static inline unsigned _msOf(TickType_t ticks) {
    return (unsigned)pdTICKS_TO_MS(ticks);
}

/**
 * @brief Message List Mutex handle - file lock table
*/

ESPMessageListMutex::ESPMessageListMutex(/* args */):_id(1), _size(0)
{
    /* Created at static-init time only: creating (not taking) a recursive mutex before the
     * scheduler runs is supported.  No logging here - the log objects are not built yet. */
    _locker = xSemaphoreCreateRecursiveMutex();
}

ESPMessageListMutex::~ESPMessageListMutex()
{
    if ((_locker != NULL) && (xSemaphoreTakeRecursive(_locker, portMAX_DELAY) == pdTRUE)) {
        _handler.clear();   /* teardown only: element mutexes are deleted by their dtor */
        _size = 0;
        xSemaphoreGiveRecursive(_locker);
    }
    _element_mutex_delete(_locker);
}

ESPMessageListMutex::Guard::Guard(ESPMessageListMutex &owner, TickType_t timeout)
    : _owner(owner), _held(false)
{
    _held = _owner._tableLock(timeout);
}

ESPMessageListMutex::Guard::~Guard()
{
    if (_held) { _owner._tableUnlock(); }
}

bool ESPMessageListMutex::_tableLock(TickType_t timeout)
{
    if (NULL == _locker) { return false; }
    return (xSemaphoreTakeRecursive(_locker, timeout) == pdTRUE);
}

void ESPMessageListMutex::_tableUnlock()
{
    if (_locker != NULL) { xSemaphoreGiveRecursive(_locker); }
}

void ESPMessageListMutex::printList() {
    /* Snapshot under the lock, log after releasing it: a log write must never be issued
     * while the file table is held (ML3). */
    struct row_t {
        char path[72];
        char src[MESSAGE_ELEMENT_LIST_SOURCE_LENGTH];
        uint32_t id;
        uint8_t counter;
        uint32_t age;
        bool owned;
    };
    /* F12: the snapshot lives on the heap instead of the caller's stack (~600 B used to sit
     * in the 4.5 KB i2c_data task).  It is copied under the table lock and logged after the
     * lock is released, and this runs only on the alert path; a failed allocation simply
     * skips the dump. */
    row_t *rows = (row_t *)malloc(sizeof(row_t) * LINKED_LIST_FILE_NUM_LIMIT);
    if (nullptr == rows) {
        MESSAGE_LIST_MUTEX_FAIL_LOG(nullptr, "%s FAIL snapshot allocation", __func__);
        return;
    }
    size_t rowsCount = 0;
    size_t size = 0;
    bool truncated = false;

    {
        Guard table(*this, LINKED_LIST_TABLE_LOCK_TIMEOUT);
        if (!table.held()) {
            MESSAGE_LIST_MUTEX_FAIL_LOG(nullptr, "%s FAIL table lock", __func__);
            free(rows);
            return;
        }
        size = _size;
        for (const auto &element : _handler) {
            if (!element.active()) { continue; }
            if (rowsCount >= (size_t)LINKED_LIST_FILE_NUM_LIMIT) { truncated = true; break; }
            row_t &row = rows[rowsCount++];
            memset(row.path, 0, sizeof(row.path));
            if (element.pbuf != nullptr) {
                strncpy(row.path, element.pbuf, sizeof(row.path) - 1);
            }
            memcpy(row.src, element.src, sizeof(row.src));
            row.id = element.id;
            row.counter = element.counter;
            row.age = millis() - element.timeStamp;
            row.owned = (element.owner != nullptr);
        }
    }

    MESSAGE_LIST_MUTEX_TAG_LOG("List elements: %u%s", (unsigned)size, truncated ? " (truncated)" : "");
    for (size_t i = 0; i < rowsCount; ++i) {
        MESSAGE_LIST_MUTEX_TAG_LOG("Element: %s, Source: %s, id: %u, counter: %u, lifetime: %u(ms), owner: %u",
                                   rows[i].path, rows[i].src, rows[i].id, rows[i].counter, rows[i].age,
                                   (unsigned)(rows[i].owned ? 1 : 0));
    }
    free(rows);   /* F12: heap snapshot */
}

message_element_list_mutex_t *ESPMessageListMutex::_find(const char *buffer) {
    if (NULL == buffer) { return nullptr; }
    /* Bounded even here: when the caller already holds the table lock this is a nested
     * acquisition and cannot fail (ML3). */
    Guard table(*this, LINKED_LIST_TABLE_LOCK_TIMEOUT);
    if (!table.held()) { return nullptr; }
    const auto iter = std::find_if(std::begin(_handler), std::end(_handler),
                                   [buffer](const message_element_list_mutex_t &c)
                                   { return (c.pbuf != nullptr) && (strcmp(c.pbuf, buffer) == 0); });
    if (iter == std::end(_handler)) {
        return nullptr;
    }
    return &(*iter); // can't use the interator to return
}

/* Claim a free slot.  A slot keeps its mutex for the whole runtime; only the path buffer
 * is allocated/freed per use, so no vSemaphoreDelete() can ever hit a live mutex. */
message_element_list_mutex_t *ESPMessageListMutex::_allocSlot(mutex_status_t *status,
                                                              bool *reclaimed) {
    /* Reusable node = inactive and not poisoned (mutex creation failed earlier); poisoned
     * nodes are dropped here, their destructor is safe. */
    auto takeFreeNode = [this]() -> message_element_list_mutex_t * {
        for (auto iter = _handler.begin(); iter != _handler.end(); ) {
            if (iter->active()) { ++iter; continue; }
            if (NULL == iter->lock) { iter = _handler.erase(iter); continue; }
            return &(*iter);
        }
        return nullptr;
    };

    /* F3: no reclaim sweep here (see _reclaimAbandoned).  A row is only recycled through the
     * normal release path (counter == 0 && owner == NULL); a row whose owner never returns
     * stays held and is surfaced by the stale alert + printList(). */
    message_element_list_mutex_t *element = takeFreeNode();
    if (element != nullptr) { return element; }
    (void)reclaimed;

    /* Every node is active, so a NEW path needs one more row: only here is the table
     * genuinely full.  A path that already has a row never reaches this point (MED-2). */
    if (_handler.size() >= (size_t)LINKED_LIST_FILE_NUM_LIMIT) {
        if (status) { *status = MUTEX_STATUS_LIST_FULL; }
        return nullptr;
    }
    try {
        _handler.emplace_back();
    } catch (const std::bad_alloc &) {
        if (status) { *status = MUTEX_STATUS_ALLOC_FAIL; }
        return nullptr;
    }
    element = &_handler.back();
    if (NULL == element->lock) {
        _handler.pop_back();
        if (status) { *status = MUTEX_STATUS_ALLOC_FAIL; }
        return nullptr;
    }
    return element;
}

void ESPMessageListMutex::_releaseSlot(message_element_list_mutex_t *element) {
    if ((nullptr == element) || (!element->active())) { return; }
    element->deactivate();          /* frees the path copy, KEEPS element->lock */
    if (_size > 0) { --_size; }
}

/* Reclaim a row whose owning task no longer exists - INTENTIONALLY REFUSED (F3).
 *
 * The old implementation probed eTaskGetState(owner) == eDeleted.  FreeRTOS only reports
 * eDeleted while the TCB is still linked in xTasksWaitingTermination (between vTaskDelete()
 * and the idle task freeing it), whereas the probe site is gated on a 5-minute live-time
 * overrun - so it normally reads memory FreeRTOS has already freed.  A false "deleted"
 * would destroy a lock that a live task still holds (exactly the ML1 fault this row model
 * was fixed for), and the failure direction cannot be proven from a dangling handle.
 *
 * A deterministic reclaim would need either (a) an API that enumerates live tasks -
 * uxTaskGetSystemState() is compiled out of this build (CONFIG_FREERTOS_USE_TRACE_FACILITY
 * is unset) - or (b) an in-use token the owner clears itself while exiting, which a
 * force-deleted task cannot do either.  The module therefore reports instead of guessing:
 * a row held far beyond LINKED_LIST_LIVE_TIME_MS is surfaced by the "held longer than ..."
 * alert and by printList() with its [src] tag.  In-tree this costs nothing: the firmware's
 * task protocol parks every task where it holds no lock (web485_v3.ino:264-277), so a row
 * outliving its owner is a leak signal for the operator, not a case to auto-heal. */
bool ESPMessageListMutex::_reclaimAbandoned(message_element_list_mutex_t *element) {
    (void)element;
    return false;
}

message_element_list_mutex_t *ESPMessageListMutex::_insert(const char *buffer, const char *src,
                                                          mutex_status_t *status, bool *staleHeld,
                                                          bool *reclaimed) {
    Guard table(*this, LINKED_LIST_TABLE_LOCK_TIMEOUT);
    if (!table.held()) {
        if (status) { *status = MUTEX_STATUS_TABLE_BUSY; }
        return nullptr;
    }
    message_element_list_mutex_t *element = _find(buffer);

    if (element) {
        const uint32_t liveTime = millis() - element->timeStamp;
        if ((liveTime > LINKED_LIST_LIVE_TIME_MS) && (staleHeld != nullptr)) {
            /* F3: REPORTED, never reclaimed - see _reclaimAbandoned().  The counter path
             * recycles the slot the moment its last in-flight take ends, so the only case
             * reaching this branch is a holder that never gives. */
            *staleHeld = true;
        }
    }

    if (!element) {
        element = _allocSlot(status, reclaimed);
        if (nullptr == element) { return nullptr; }  /* status set by _allocSlot() */
        if (!element->activate(buffer, src)) {
            if (status) { *status = MUTEX_STATUS_ALLOC_FAIL; }
            return nullptr;
        }
        element->id = _id++;
        ++_size;
    }
    return element;
}

bool ESPMessageListMutex::takeEx(const char *buffer, TickType_t xTicksToWait,
                                 const char *src, mutex_status_t *reason) {
    mutex_status_t st = MUTEX_STATUS_OK;
    const TaskHandle_t self = xTaskGetCurrentTaskHandle();
    const TickType_t start = xTaskGetTickCount();
    /* Phase 1 retries inside its own budget, never forever, even for portMAX_DELAY. */
    const TickType_t admissionBudget =
        ((xTicksToWait == portMAX_DELAY) || (xTicksToWait > LINKED_LIST_ADMISSION_WAIT_MAX))
            ? LINKED_LIST_ADMISSION_WAIT_MAX : xTicksToWait;
    message_element_list_mutex_t *element = nullptr;
    bool status = false;
    bool staleHeld = false;
    bool reclaimed = false;
    uint32_t id = 0;
    uint8_t counterAfter = 0;
    size_t sizeAfter = 0;

    if (NULL == buffer) { st = MUTEX_STATUS_ALLOC_FAIL; goto done; }

    /* Phase 1 - reserve one admission slot for [buffer].  counter == holders (including
     * recursion of the same task) + tasks waiting on that file, so it also pins the slot:
     * a row that is reserved can no longer be recycled by another task in between. */
    for (;;) {
        bool reserved = false;
        {
            Guard table(*this, LINKED_LIST_TABLE_LOCK_TIMEOUT);
            if (!table.held()) { st = MUTEX_STATUS_TABLE_BUSY; goto done; }
            sizeAfter = _size;

            st = MUTEX_STATUS_OK;
            element = _insert(buffer, src, &st, &staleHeld, &reclaimed);
            if (nullptr == element) {
                /* LIST_FULL is retryable: another task may release a row inside this
                 * caller's budget.  ALLOC_FAIL / TABLE_BUSY are not (MED-2). */
                if (st != MUTEX_STATUS_LIST_FULL) { goto done; }
            } else if ((element->owner == self) ||
                       (element->counter < LINKED_LIST_RECURSIVE_MUTEX_LIMIT)) {
                /* Saturate instead of wrapping: counter is uint8_t and the owner bypass
                 * above makes this increment unconditional for a nested take. */
                if (element->counter < 0xFF) { ++element->counter; }
                counterAfter = element->counter;
                id = element->id;
                sizeAfter = _size;
                reserved = true;
            } else {
                st = MUTEX_STATUS_ADMISSION_LIMIT;  /* retryable: a slot frees on give/timeout */
            }
        }
        if (reserved) { break; }

        /* No free row / no admission slot yet: wait inside the admission budget instead of
         * refusing instantly and letting the caller drop the work (ML6).  Bounded by
         * LINKED_LIST_ADMISSION_WAIT_MAX so a huge caller timeout cannot turn this into a
         * long poll storm; on expiry [st] already holds the retryable reason. */
        if (_tickExpired(start, admissionBudget)) { goto done; }
        vTaskDelay(1);
    }

    if (reclaimed) {
        MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s abandoned row reclaimed (owner task was deleted)",
                                    (src != nullptr) ? src : "", __func__, buffer);
    }
    if (staleHeld) {
        MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s held longer than %u(ms) but still in use: not reclaimed",
                                    (src != nullptr) ? src : "", __func__, buffer,
                                    (unsigned)LINKED_LIST_LIVE_TIME_MS);
    }

    /* Phase 2 - wait for the per-file mutex.  The table lock is NOT held here, so other
     * paths keep working while this task waits (the old code had to unlock it by hand). */
    status = _element_mutex_take(element->lock, _tickRemaining(start, xTicksToWait));

    /* Phase 3 - bookkeeping.  F4: the table section is memory-only and the wait is now
     * BOUNDED (100 ms) like every other acquisition in the module; the old portMAX_DELAY
     * could wedge the path forever if a task died inside a table section.  When the table
     * cannot be locked the mutex is handed straight back AND the phase-1 reservation is
     * released, then MUTEX_STATUS_TABLE_BUSY is reported (the caller drops the work). */
    {
        Guard table(*this, LINKED_LIST_TABLE_LOCK_TIMEOUT);
        if (!table.held()) {
            /* Unreachable while _locker exists (phase 1 would have failed).  Never claim a
             * lock whose ownership cannot be recorded: hand the mutex straight back AND give
             * back the phase-1 reservation, exactly like the timeout branch below - leaving
             * the pin behind would keep the row from ever reaching counter == 0. */
            if (status) { _element_mutex_give(element->lock); status = false; }
            if (element->counter > 0) { --element->counter; }
            if ((0 == element->counter) && (nullptr == element->owner)) {
                _releaseSlot(element);
            }
            counterAfter = element->counter;
            sizeAfter = _size;
            st = MUTEX_STATUS_TABLE_BUSY;
            goto done;
        }
        if (status) {
            element->timeStamp = millis();
            if (element->owner == self) {
                if (element->ownerDepth < 0xFFFF) { ++element->ownerDepth; }
            } else {
                element->owner = self;
                element->ownerDepth = 1;
            }
            st = MUTEX_STATUS_OK;
        } else {
            /* Timed out: give back only this task's reservation.  The slot is released
             * only when nobody holds it and nobody waits any more (ML1). */
            if (element->counter > 0) { --element->counter; }
            if ((0 == element->counter) && (nullptr == element->owner)) {
                _releaseSlot(element);
            }
            st = MUTEX_STATUS_TIMEOUT;
        }
        counterAfter = element->counter;
        sizeAfter = _size;
    }

done:
    if (buffer != nullptr) {
        const unsigned waitedMs = _msOf(_tickElapsed(start));
        switch (st) {
        case MUTEX_STATUS_LIST_FULL:
            MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s FAIL over size = %u",
                                        (src != nullptr) ? src : "", __func__, buffer, (unsigned)sizeAfter);
            break;
        case MUTEX_STATUS_ADMISSION_LIMIT:
            MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s FAIL over admission limit = %u after %u(ms)",
                                        (src != nullptr) ? src : "", __func__, buffer,
                                        (unsigned)LINKED_LIST_RECURSIVE_MUTEX_LIMIT, waitedMs);
            break;
        case MUTEX_STATUS_TABLE_BUSY:
            MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s FAIL table lock after %u(ms)",
                                        (src != nullptr) ? src : "", __func__, buffer, waitedMs);
            break;
        case MUTEX_STATUS_ALLOC_FAIL:
            MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s FAIL allocation",
                                        (src != nullptr) ? src : "", __func__, buffer);
            break;
        case MUTEX_STATUS_TIMEOUT:
            /* F1: the log path is filtered for every failure branch by the macro itself. */
            MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s FAIL to take mutex, timeout %u, counter = %u, waited = %u(ms), id = %u, size = %u",
                                        (src != nullptr) ? src : "", __func__, buffer,
                                        (unsigned)xTicksToWait, counterAfter, waitedMs,
                                        (unsigned)id, (unsigned)sizeAfter);
            break;
        default:
            break;
        }
    }
    MESSAGE_LIST_MUTEX_CONSOLE("%s %s %s %s counter = %u, id = %u, size = %u",
                               (src != nullptr) ? src : "", __func__, (buffer != nullptr) ? buffer : "(null)",
                               status ? "OK" : "FAIL", counterAfter, (unsigned)id, (unsigned)sizeAfter);
    if (reason) { *reason = st; }
    return status;
}

bool ESPMessageListMutex::take(const char *buffer, TickType_t xTicksToWait, const char *src) {
    return takeEx(buffer, xTicksToWait, src, nullptr);
}

bool ESPMessageListMutex::take(String &buffer, TickType_t xTicksToWait, const char *src) {
    return takeEx(buffer.c_str(), xTicksToWait, src, nullptr);
}

bool ESPMessageListMutex::giveEx(const char *buffer, const char *src, mutex_status_t *reason) {
    mutex_status_t st = MUTEX_STATUS_OK;
    const TaskHandle_t self = xTaskGetCurrentTaskHandle();
    bool status = false;
    bool notOwner = false;
    bool notFound = false;
    bool released = false;
    uint32_t id = 0;
    uint8_t counterAfter = 0;
    size_t sizeAfter = 0;

    if (NULL == buffer) { st = MUTEX_STATUS_ALLOC_FAIL; goto done; }

    {
        /* F4: bounded table wait, like every other acquisition in the module.  The section is
         * memory-only, so 100 ms is far beyond the longest legitimate hold; on failure the
         * release is REPORTED as TABLE_BUSY (loud, always-on) instead of stalling the caller
         * forever.  The caller keeps its lock in that case - the alert path (printList /
         * "held longer than") surfaces the row, and the old design chose an infinite wait to
         * avoid exactly this drop. */
        Guard table(*this, LINKED_LIST_TABLE_LOCK_TIMEOUT);
        if (!table.held()) { st = MUTEX_STATUS_TABLE_BUSY; goto done; }

        message_element_list_mutex_t *element = _find(buffer);
        if (nullptr == element) {
            /* The key matches no live row: either this path was already released, or the
             * caller's path buffer was mutated/freed while it blocked here (ML7).  A
             * mis-keyed give is REPORTED, never guessed at - releasing "whatever element
             * this task happens to hold" could drop a lock that is still in use, which is
             * strictly worse than the leaked lock it would rescue. */
            notFound = true;
            st = MUTEX_STATUS_NOT_FOUND;
            goto done;
        }
        if (element->owner != self) {
            /* ML2: the calling task does not hold this file.  The counter is not touched
             * and the slot is not erased, so the real holder keeps its lock. */
            notOwner = true;
            id = element->id;
            counterAfter = element->counter;
            sizeAfter = _size;
            st = MUTEX_STATUS_NOT_OWNER;
            goto done;
        }

        id = element->id;
        /* FreeRTOS refuses the release when the caller is not the holder, so the counter
         * and the slot are only touched on a release that really happened (ML2). */
        status = _element_mutex_give(element->lock);
        if (status) {
            if (element->ownerDepth > 0) { --element->ownerDepth; }
            if (0 == element->ownerDepth) { element->owner = nullptr; }
            if (element->counter > 0) { --element->counter; }
            element->timeStamp = millis();
            if ((0 == element->counter) && (nullptr == element->owner)) {
                _releaseSlot(element);
                released = true;
            }
            st = MUTEX_STATUS_OK;
        } else {
            notOwner = true;
            st = MUTEX_STATUS_NOT_OWNER;
        }
        counterAfter = element->counter;
        sizeAfter = _size;
    }

done:
    if (notFound) {
        MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s element isn't exist", (src != nullptr) ? src : "", __func__,
                                    (buffer != nullptr) ? buffer : "(null)");
    } else if (notOwner) {
        MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s NOT OWNER - release refused, id = %u, counter = %u, size = %u",
                                    (src != nullptr) ? src : "", __func__,
                                    (buffer != nullptr) ? buffer : "(null)",
                                    (unsigned)id, counterAfter, (unsigned)sizeAfter);
    } else if (st == MUTEX_STATUS_TABLE_BUSY) {
        /* F4: the release could not be recorded within the bounded table wait. */
        MESSAGE_LIST_MUTEX_FAIL_LOG(buffer, "%s %s %s FAIL table lock - release NOT performed",
                                    (src != nullptr) ? src : "", __func__,
                                    (buffer != nullptr) ? buffer : "(null)");
    }
    MESSAGE_LIST_MUTEX_CONSOLE("%s %s %s %s counter = %u, id = %u, size = %u, released = %u",
                               (src != nullptr) ? src : "", __func__, (buffer != nullptr) ? buffer : "(null)",
                               status ? "OK" : "FAIL", counterAfter, (unsigned)id, (unsigned)sizeAfter,
                               (unsigned)(released ? 1 : 0));
    (void)released; /* only reported by the (normally disabled) console trace */
    if (reason) { *reason = st; }
    return status;
}

bool ESPMessageListMutex::give(const char *buffer, const char *src) {
    return giveEx(buffer, src, nullptr);
}

bool ESPMessageListMutex::give(String &buffer, const char *src) {
    /* Was missing its return statement: a bool function that fell off the end (ML gap). */
    return giveEx(buffer.c_str(), src, nullptr);
}

bool ESPMessageListMutex::isLocked(const char *buffer) {
    return _find(buffer) != nullptr;
}

size_t ESPMessageListMutex::count(bool *ok) {
    Guard table(*this, LINKED_LIST_TABLE_LOCK_TIMEOUT);
    if (!table.held()) {
        /* Never let a monitor read a locked-out table as "0 rows in use": report it and
         * return a value that keeps the alert condition TRUE (fail-loud). */
        if (ok) { *ok = false; }
        MESSAGE_LIST_MUTEX_TAG_LOG("%s FAIL table lock - count is INVALID (reported as SIZE_MAX)", __func__);
        return (size_t)-1;
    }
    if (ok) { *ok = true; }
    return _size;
}

/* F2 monitor: age of the oldest row that is currently OWNED (0 when none).  Copy of the
 * printList() lifetime column, exposed so the alert path can trigger on a long-held row. */
size_t ESPMessageListMutex::oldestHeldMs() {
    Guard table(*this, LINKED_LIST_TABLE_LOCK_TIMEOUT);
    if (!table.held()) { return 0; }
    const uint32_t nowMs = millis();
    uint32_t oldest = 0;
    for (const auto &element : _handler) {
        if ((!element.active()) || (nullptr == element.owner)) { continue; }
        const uint32_t age = nowMs - element.timeStamp;
        if (age > oldest) { oldest = age; }
    }
    return oldest;
}

message_element_list_mutex::message_element_list_mutex() {
    /* Every member is initialised here: the old _element_mutex_init(lock, true) read an
     * uninitialised handle and never checked the creation result (ML9). */
    pbuf = nullptr;
    src[0] = '\0';
    lock = nullptr;
    id = 0;
    timeStamp = millis();
    counter = 0;
    owner = nullptr;
    ownerDepth = 0;
    lock = xSemaphoreCreateRecursiveMutex();
    /* No trace macro here: this constructor runs inside the file table section
     * (_allocSlot <- _insert <- takeEx), and a log write must never run under that lock. */
}

message_element_list_mutex::~message_element_list_mutex() {
    deactivate();
    _element_mutex_delete(lock);
}

bool message_element_list_mutex::activate(const char *buffer, const char *source) {
    if ((NULL == lock) || (NULL == buffer)) { return false; }

    const size_t length = strlen(buffer) + 1;
    char *copy = new (std::nothrow) char[length];
    if (NULL == copy) { return false; }
    memcpy(copy, buffer, length);

    if (pbuf != nullptr) { delete[] pbuf; }
    pbuf = copy;
    counter = 0;
    owner = nullptr;
    ownerDepth = 0;
    timeStamp = millis();
    strncpy(src, (source != nullptr) ? source : "", sizeof(src) - 1);
    src[sizeof(src) - 1] = '\0'; // ensure null termination
    /* No trace macro here either: activate() is called from _insert() while the file table
     * lock is held. */
    return true;
}

void message_element_list_mutex::deactivate() {
    if (pbuf != nullptr) {
        delete[] pbuf;
        pbuf = nullptr;
    }
    counter = 0;
    owner = nullptr;
    ownerDepth = 0;
    src[0] = '\0';
}

/**
 * @brief Message List handle
*/
ESPMessageList::ESPMessageList(const char *element) : _elementName(element) {
    _elementName += "_";
    _elementName += (unsigned int)l_counter.fetch_add(1) + 1U;
}

ESPMessageList::~ESPMessageList() {
    end();
}

void ESPMessageList::end() {
    std::lock_guard<std::recursive_mutex> lock(_locker);
    _handler.clear();
    _inFlight.clear();
}

bool ESPMessageList::bufferSend(const uint8_t *pvItem, size_t xItemSize, void *arg) {
    (void)arg;
    if (!pvItem || (0 == xItemSize)) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> lock(_locker);
    if (_handler.size() >= (size_t)MESSAGE_LIST_ELEMENTS_LIMIT) { return false; }
    try {
        _handler.emplace_back(pvItem, xItemSize);
    } catch (const std::bad_alloc &) {
        return false;
    }
    message_element_list_t *element = &_handler.back();
    if (NULL == element->pBuffer) {
        _handler.pop_back();
        return false;
    }
    return true;
}

bool ESPMessageList::bufferSend(const char *pvItem, void *arg) {
    /* Null must be rejected before strlen() is reached (ML10). */
    if (NULL == pvItem) { return false; }
    return bufferSend((const uint8_t*)pvItem, strlen(pvItem), arg);
}

void *ESPMessageList::bufferReceive(size_t *pxItemSize, void *arg) {
    (void)arg;
    std::lock_guard<std::recursive_mutex> lock(_locker);
    if (_handler.empty()) {
        return nullptr;
    }
    auto iter = _handler.begin();
    message_element_list_t &element = *iter;
    if (NULL == element.pBuffer) {
        /* A failed copy is dropped here, so bufferLength is never read on that path (ML10). */
        _handler.erase(iter);
        return nullptr;
    }
    if (pxItemSize != nullptr) { *pxItemSize = element.bufferLength; }
    void *item = (void*)element.pBuffer;
    /* Ownership moves to _inFlight: a second receiver can no longer be handed the same
     * buffer, and only the matching bufferReturnItem() frees it (ML8). */
    _inFlight.splice(_inFlight.end(), _handler, iter);
    return item;
}

bool ESPMessageList::bufferReturnItem(void *pvItem, void *arg) {
    (void)arg;
    std::lock_guard<std::recursive_mutex> lock(_locker);

    if (pvItem != nullptr) {
        const auto iter = std::find_if(std::begin(_inFlight), std::end(_inFlight),
                                       [pvItem](const message_element_list_t &c)
                                       { return pvItem == c.pBuffer; });
        if (iter == std::end(_inFlight)) {
            /* Not in flight: never free an item that is still queued or unknown, a
             * mis-paired call used to pop and free an unrelated buffer (ML8). */
            return false;
        }
        _inFlight.erase(iter);
        return true;
    }

    if (_inFlight.empty()) { return false; }
    /* No argument means "the item I received": only unambiguous when exactly one is out. */
    if (_inFlight.size() > 1) { return false; }
    _inFlight.pop_front();
    return true;
}

size_t ESPMessageList::available() {
    std::lock_guard<std::recursive_mutex> lock(_locker);
    return _handler.size();
}

bool ESPMessageList::empty() {
    std::lock_guard<std::recursive_mutex> lock(_locker);
    return _handler.empty();
}

size_t ESPMessageList::inFlight() {
    std::lock_guard<std::recursive_mutex> lock(_locker);
    return _inFlight.size();
}

/**
 * message element handle
*/
message_element_list::message_element_list(const uint8_t* buffer, size_t length) {
    if ((NULL == buffer) || (0 == length)) { return; }
    pBuffer = new (std::nothrow) uint8_t[length + 1]; // add 1byte for the null terminal
    if (pBuffer) {
        listCounterAllocated.fetch_add(1);
        bufferLength = length;
        memcpy(pBuffer, buffer, length);
        pBuffer[length] = '\0';
        ELEMENT_LIST_CONSOLE("%s constructor declaration at %p, buff = %s, elements = %u",
                            __func__, this, pBuffer, (unsigned)listCounterAllocated.load());
    }
}

message_element_list::~message_element_list() {
    if (pBuffer) {
        /* F13: clamp with a CAS loop - the old load-then-store pair was a check-then-act
         * that two concurrent destructors could both pass, wrapping the counter to
         * SIZE_MAX (diagnostic-only, but it made the dump read SIZE_MAX). */
        size_t cur = listCounterAllocated.load();
        while ((cur > 0) && (!listCounterAllocated.compare_exchange_weak(cur, cur - 1))) {
            /* compare_exchange_weak reloads `cur` on failure */
        }
        ELEMENT_LIST_CONSOLE("%s constructor definition at %p, buff = %s, elements = %u",
                                __func__, this, pBuffer, (unsigned)listCounterAllocated.load());
        delete[] pBuffer;
        pBuffer = nullptr;
    }
    bufferLength = 0;
}

ESPMessageListMutex mutexFileList;
