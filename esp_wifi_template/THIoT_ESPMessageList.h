#ifndef	__ESP_LINKED_LIST_H
#define __ESP_LINKED_LIST_H

#define C_LINKED_LIST_HANDLE    0
/*
Line 71: C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\libraries\SD_MMC\src\SD_MMC.cpp
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = format_if_mount_failed,
        .max_files = 5, ///< Max number of open files
        .allocation_unit_size = 0
    };
*/
#include <Arduino.h>
#include "THIoT_ESPSDFSClass.h"

#define LINKED_LIST_FILE_NUM_LIMIT          SDMMC_MAX_OPEN_FILES
/* Alert one step BELOW the refusal boundary (LINKED_LIST_FILE_NUM_LIMIT), so the in-tree
 * monitor can warn while the table still accepts new paths.  ML5: with the old value 5
 * the monitor could only fire at the exact size at which take() already refuses. */
#define LINKED_LIST_FILE_ALERT_NUM          4   /* F6: 3 is a legitimate steady state (the alert
                                                 * used to fire on any normal 3-path workload);
                                                 * one step below the 5-row refusal boundary. */
/* Maximum number of in-flight take() calls (holder recursion + waiting tasks) per path.
 * 5 = the deepest legitimate same-task nesting in the tree (CsvLogStore::addParams ->
 * FSULogStream::write = depth 2) plus 3 competitors.  With the previous value 3 the owner's
 * own nesting ate most of the budget, so only ONE competitor could be admitted at a time
 * and the next one had to retry until its deadline. */
#define LINKED_LIST_RECURSIVE_MUTEX_LIMIT   5

#define LINKED_LIST_FILE_MUTEX_TIMEOUT_10S  pdMS_TO_TICKS(1000U * 10) // 10 seconds
#define LINKED_LIST_FILE_MUTEX_TIMEOUT_1S   pdMS_TO_TICKS(1000U * 1) // 1 seconds
#define LINKED_LIST_FILE_MUTEX_TIMEOUT_S(x) pdMS_TO_TICKS(1000U * (x)) // x seconds
#define LINKED_LIST_FILE_MUTEX_TIMEOUT_MS(x) pdMS_TO_TICKS(x) // x milliseconds
#define LINKED_LIST_FILE_MUTEX_TIMEOUT_MAX  portMAX_DELAY
/* Bounded default: a caller that forgets the timeout must still not wait forever (ML6). */
#define LINKED_LIST_FILE_MUTEX_TIMEOUT_DEFAULT  LINKED_LIST_FILE_MUTEX_TIMEOUT_1S
/* Budget for the file table lock.  The table section only touches memory (no FS I/O, no
 * logging runs under it), so this is a fail-fast guard, not a latency factor (ML3). */
#define LINKED_LIST_TABLE_LOCK_TIMEOUT      pdMS_TO_TICKS(100)
/* Own budget for the phase-1 retry loop (waiting for a free row / an admission slot).
 * Deliberately separate from the caller's timeout: a caller that passes portMAX_DELAY, or
 * a very large budget, must never poll the table forever (10 s used to mean ~10.000
 * vTaskDelay(1) round trips).  Whatever is left of the caller budget afterwards still goes
 * to the file-mutex wait in phase 2. */
#define LINKED_LIST_ADMISSION_WAIT_MAX      pdMS_TO_TICKS(100)

/* F2 monitor: a row held longer than this is logged by the monitor path together with its
 * [src] tag, so a leaked row (missed give) is visible long before the 5-minute stale warning. */
#define LINKED_LIST_HELD_ALERT_MS           (30U * 1000U)

/* Monitor log rate limit: while an alert condition stays true the in-tree monitor runs once per
 * i2c_data iteration (about every 1-2 s, driven by the pump/card report tickers) and every dump
 * writes ~10 records into /LOG_TRACE.TXT - including the very file whose lock row the dump is
 * reporting.  The condition itself is still evaluated on every iteration; only the report is
 * written at most once per this interval (first alert logs immediately). */
#define LINKED_LIST_ALERT_LOG_INTERVAL_MS   (5U * 1000U)

/* Why a take()/give() failed.  take()/give() keep returning bool for the existing call
 * sites; the *_ex() forms report the reason so a caller can stop treating "file busy",
 * "table full", "out of memory" and "table lock busy" as one and the same (ML5). */
typedef enum {
    MUTEX_STATUS_OK = 0,
    MUTEX_STATUS_TABLE_BUSY,        /* could not lock the file table within the budget */
    MUTEX_STATUS_LIST_FULL,         /* LINKED_LIST_FILE_NUM_LIMIT different paths live */
    MUTEX_STATUS_ADMISSION_LIMIT,   /* LINKED_LIST_RECURSIVE_MUTEX_LIMIT in-flight takes */
    MUTEX_STATUS_TIMEOUT,           /* waited for the file mutex and the deadline expired */
    MUTEX_STATUS_ALLOC_FAIL,        /* mutex or path copy could not be allocated */
    MUTEX_STATUS_NOT_FOUND,         /* give(): no element for this path/key */
    MUTEX_STATUS_NOT_OWNER          /* give(): the calling task does not hold that file */
} mutex_status_t;

const char *mutexStatusToString(mutex_status_t status);

#if (C_LINKED_LIST_HANDLE == 1)
#include "THIoT_PFcList.h"
#else
#include <mutex>
#include <list>
#endif

#define MESSAGE_ELEMENT_LIST_SOURCE_LENGTH  32

/* File-lock RULE (F5/F6, documented contract): the module guarantees mutual exclusion for
 * the single path key a caller passes to take()/give().  Paths an operation also mutates
 * (rename destinations, mkdir parents, derived backup/archive names) are NOT covered by that
 * key - callers either hold those keys themselves or accept the documented gap.  The 5-row
 * table is a PATH-LOCK cap, not a VFS descriptor cap: a lock holder can still fail fs->open()
 * when the FS runs out of descriptors (SDMMC_MAX_OPEN_FILES). */
/* One row of the file lock table.
 *
 * Lifetime rule (fixes ML1/ML2/ML4/ML9): the FreeRTOS mutex of a row is created once and
 * is NOT destroyed while the firmware runs.  A row that goes idle only releases its path
 * buffer and stays in the table as a free slot that a later path reuses.  Therefore no
 * task can ever lose the mutex it holds or is waiting on to a vSemaphoreDelete() issued
 * for another path - which is what used to corrupt the heap and break mutual exclusion.
 */
typedef struct message_element_list_mutex {
    char *pbuf;                     /* owned copy of the path; NULL => free slot */
    char src[MESSAGE_ELEMENT_LIST_SOURCE_LENGTH];
    SemaphoreHandle_t lock;         /* recursive, created once, reused, released on deactivate */
    uint32_t id;
    uint32_t timeStamp;             /* last successful take() or give() */
    volatile uint8_t counter;       /* in-flight take() calls: holders (incl. recursion) + waiters */
    TaskHandle_t owner;             /* task currently holding lock, NULL when free */
    uint16_t ownerDepth;            /* recursion depth of owner inside lock */
    message_element_list_mutex();
    ~message_element_list_mutex();
    /* F9: a raw owning pointer plus a destructor means a copy would double-free the path and
     * the mutex.  Every container use is copy-free (emplace_back / splice / erase / clear), so
     * the copy and assignment operators are deleted to make a future `push_back(other)` fail
     * at compile time instead of corrupting the heap. */
    message_element_list_mutex(const message_element_list_mutex &) = delete;
    message_element_list_mutex &operator=(const message_element_list_mutex &) = delete;
    bool active() const { return pbuf != nullptr; }
    bool activate(const char *buffer, const char *source);  /* claim a free slot for [buffer] */
    void deactivate();                                      /* release the slot, KEEP the mutex */
} message_element_list_mutex_t;

class ESPMessageListMutex
{
private:
#if (C_LINKED_LIST_HANDLE == 1)
    c_list_t* list_fp_handler;
    SemaphoreHandle_t _mutex_list;
#else
    std::list<message_element_list_mutex_t> _handler;
    /* Recursive FreeRTOS mutex with a bounded wait: std::recursive_mutex had no timeout
     * and no failure path at all (ML3).  Declared mutable-free: every user holds it. */
    SemaphoreHandle_t _locker;
#endif
    uint32_t _id;
    size_t _size;                   /* number of ACTIVE rows (free slots excluded) */

    /* Scoped lock over the file table.  Nothing blocking may run while it is held. */
    class Guard
    {
    public:
        Guard(ESPMessageListMutex &owner, TickType_t timeout);
        ~Guard();
        bool held() const { return _held; }
    private:
        ESPMessageListMutex &_owner;
        bool _held;
    };

    bool _tableLock(TickType_t timeout);
    void _tableUnlock();
    /* _find()/_allocSlot()/_releaseSlot()/activate require the caller to hold the table lock;
     * only _insert() takes the recursive table lock itself (F10: the old comment claimed the
     * helpers take it recursively, which _allocSlot never did). */
    message_element_list_mutex_t *_find(const char *buffer);
    /* Reports MUTEX_STATUS_LIST_FULL only when a NEW row is required (MED-2), after a sweep
     * that tries to reclaim abandoned rows of every path first. */
    message_element_list_mutex_t *_allocSlot(mutex_status_t *status, bool *reclaimed);
    message_element_list_mutex_t *_insert(const char *buffer, const char *src,
                                         mutex_status_t *status, bool *staleHeld, bool *reclaimed);
    void _releaseSlot(message_element_list_mutex_t *element);
    /* Reclaim a row whose owning task no longer exists.  Only provable when every
     * in-flight take belongs to that dead owner (counter == ownerDepth), i.e. nobody holds
     * or waits on the mutex; the mutex is then replaced, never reused.  Table lock held. */
    bool _reclaimAbandoned(message_element_list_mutex_t *element);
public:
    ESPMessageListMutex(/* args */);
    ~ESPMessageListMutex();
    void printList();
    bool take(const char *buffer, TickType_t xTicksToWait = LINKED_LIST_FILE_MUTEX_TIMEOUT_DEFAULT, const char *src = "");
    bool take(String &buffer, TickType_t xTicksToWait = LINKED_LIST_FILE_MUTEX_TIMEOUT_DEFAULT, const char *src = "");
    bool give(const char *buffer, const char *src = "");
    bool give(String &buffer, const char *src = "");
    /* Reason-reporting variants.  [reason] may be NULL. */
    bool takeEx(const char *buffer, TickType_t xTicksToWait, const char *src, mutex_status_t *reason);
    bool giveEx(const char *buffer, const char *src, mutex_status_t *reason);
    /* Returns false both for "not locked" and for "table could not be locked" (this API is
     * unused today; count(&ok) reports the distinction). */
    bool isLocked(const char *buffer);
    /* [ok] (optional) is set false when the file table could not be locked, in which case
     * the returned count is SIZE_MAX - a monitor must never read that as "0 rows". */
    size_t count(bool *ok = nullptr);
    /* F2 monitor: age of the oldest row currently OWNED (0 when none).  The alert path prints
     * it, so a row leaked by a missed give (editor transfer without a disconnect callback) is
     * visible long before the 5-minute stale warning. */
    size_t oldestHeldMs();
};

typedef struct message_element_list {
    size_t bufferLength = 0;        /* ML10: always initialised, even when the copy fails */
    uint8_t *pBuffer = nullptr;
    message_element_list(const uint8_t *buffer, size_t length);
    ~message_element_list();
    /* F9: see message_element_list_mutex above - the owning pBuffer must never be copied. */
    message_element_list(const message_element_list &) = delete;
    message_element_list &operator=(const message_element_list &) = delete;
} message_element_list_t;

class ESPMessageList
{
private:
    std::list<message_element_list_t> _handler;   /* queued items */
    /* Items already handed out by bufferReceive().  They live here until the matching
     * bufferReturnItem(), so two receivers can never be given the same buffer (ML8). */
    std::list<message_element_list_t> _inFlight;
    mutable std::recursive_mutex _locker;
    String _elementName;

public:
    ESPMessageList(const char *element = "");
    ~ESPMessageList();
    void end();
    bool bufferSend(const uint8_t *pvItem, size_t xItemSize, void *arg = nullptr);
    bool bufferSend(const char *pvItem, void *arg = nullptr);
    void *bufferReceive(size_t *pxItemSize, void *arg = nullptr);
    bool bufferReturnItem(void *pvItem = nullptr, void *arg = nullptr);
    /* Defined in the .cpp: they read the shared list, so they lock it (ML8). */
    size_t available();
    bool empty();
    size_t inFlight();
};

extern ESPMessageListMutex mutexFileList;
#endif // __ESP_LINKED_LIST_H
