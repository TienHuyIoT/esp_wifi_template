#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPMessageQueue.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPTaskHelper.h"

#if (1)
#define QUEUE_HANDLE_TAG_LOG(...)           FS_TAG_LOGI("[QUEUE_HANDLE]", __VA_ARGS__)
#else
#define QUEUE_HANDLE_TAG_LOG(...)           SERIAL_TAG_LOGI("[QUEUE_HANDLE]", __VA_ARGS__)
#endif

#if (QUEUE_BUFFER_DEBUG == 1)
#define QUEUE_HANDLE_TAG_CONSOLE_I(...)     SERIAL_TAG_LOGI("[QUEUE_HANDLE]", __VA_ARGS__)
#define QUEUE_HANDLE_TAG_CONSOLE_W(...)     SERIAL_TAG_LOGW("[QUEUE_HANDLE]", __VA_ARGS__)
#else
#define QUEUE_HANDLE_TAG_CONSOLE_I(f_, ...)
#define QUEUE_HANDLE_TAG_CONSOLE_W(f_, ...)
#endif

/* Q2: always-compiled failure/warning channels.  The debug-gated CONSOLE macros above are
 * empty in the shipped build, so creation/allocation/send failures used to be silent
 * (MQ14).  FS_TAG_LOGx writes to the file log (and E/W are on in the default level).
 *
 * N1 (fix-review round): the file log itself enqueues into `_log_trace_queue` - the very
 * queue that can be the one failing - so an unguarded failure log re-enters bufferSend
 * recursively (the cycle is broken only by the log module's 500 ms soft reset).  The guard
 * below is same-task: the log write happens synchronously in the caller's context, so a
 * nested failure is counted but never logged.  A cross-task collision merely costs one
 * suppressed diagnostic, which is the right trade for a module whose diagnostics must not
 * travel on the transport they diagnose. */
static volatile bool _queueDiagBusy = false;

#define QUEUE_HANDLE_TAG_ERROR(...)                                             \
    do {                                                                        \
        if (!_queueDiagBusy) {                                                  \
            _queueDiagBusy = true;                                              \
            FS_TAG_LOGE("[QUEUE_HANDLE]", __VA_ARGS__);                         \
            _queueDiagBusy = false;                                             \
        }                                                                       \
    } while (0)
#define QUEUE_HANDLE_TAG_WARN(...)                                              \
    do {                                                                        \
        if (!_queueDiagBusy) {                                                  \
            _queueDiagBusy = true;                                              \
            FS_TAG_LOGW("[QUEUE_HANDLE]", __VA_ARGS__);                         \
            _queueDiagBusy = false;                                             \
        }                                                                       \
    } while (0)

/* Q3 / MQ2: one module-wide recursive lock.  It guards the registry vector and every short
 * per-object state update (counters, in-flight set, closing flags).  It is NEVER held across
 * a blocking queue operation.  Created at static-init time, like mutexFileList in
 * THIoT_ESPMessageList.cpp. */
static SemaphoreHandle_t _queueLock = xSemaphoreCreateRecursiveMutex();
#define QUEUE_LOCK()        do { if (_queueLock) { xSemaphoreTakeRecursive(_queueLock, portMAX_DELAY); } } while (0)
#define QUEUE_UNLOCK()      do { if (_queueLock) { xSemaphoreGiveRecursive(_queueLock); } } while (0)

/* Q3 / MQ1: hard cap of the deferred-delete retries.  N3: the cadence and the cap are chosen
 * so the total (6 s) strictly exceeds 2x the longest accepted send wait (the FS editor's 2 s),
 * otherwise the cap can expire while a sender is still inside its bounded wait. */
#define TASK_DELETE_DEFER_INTERVAL_MS   100U
#define TASK_DELETE_DEFER_MAX           60U   /* 60 x 100 ms = 6 s >= 2 x 2000 ms */

extern ESPMessageQueue i2c_pumpLogQueueHandle;
extern ESPMessageQueue i2c_eventDataQueueHandle;
extern ESPMessageQueue _log_trace_queue;
#if (gConsoleDebugLevel > 0)
extern ESPMessageQueue _console_trace_queue;
#endif
std::vector<ESPMessageQueue *> _messageQueueHandlers;

#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 1)
typedef const uint8_t* pMessage_t;
typedef struct {
    pMessage_t pBuffer;
    size_t length;
} messageHandle_t;

static size_t e_counter = 0;
static volatile size_t queueCounterAllocated = 0;

size_t messageQueueAllocated() {
    QUEUE_LOCK();
    const size_t value = queueCounterAllocated;
    QUEUE_UNLOCK();
    return value;
}

/* Q3 / MQ2: snapshot the registry under the lock and log AFTER releasing it - a log write
 * must never run under a module lock (and async_tcp must not inherit the wait). */
void recordMessageQueueHandlers() {
    struct row_t {
        char name[40];
        size_t alloc;
        size_t cap;
        size_t drop;
        size_t dropBytes;
    };
    row_t rows[8];
    size_t count = 0;
    size_t totalAllocated = 0;
    size_t gauge = 0;
    size_t registered = 0;

    QUEUE_LOCK();
    gauge = queueCounterAllocated;
    registered = _messageQueueHandlers.size();
    for (auto &queue : _messageQueueHandlers) {
        if ((!queue) || (count >= (sizeof(rows) / sizeof(rows[0])))) { continue; }
        row_t &r = rows[count++];
        strncpy(r.name, queue->elementName().c_str(), sizeof(r.name) - 1);
        r.name[sizeof(r.name) - 1] = '\0';
        r.alloc = queue->allocated();
        r.cap = queue->capacity();
        r.drop = queue->dropped();
        r.dropBytes = queue->droppedBytes();
        totalAllocated += r.alloc;
    }
    QUEUE_UNLOCK();

    QUEUE_HANDLE_TAG_LOG("MessageQueueHandler count: %u", (unsigned)gauge);
    for (size_t i = 0; i < count; ++i) {
        QUEUE_HANDLE_TAG_LOG("MessageQueueHandler: %s, allocated: %u/%u, dropped: %u (%u B)",
                             rows[i].name, (unsigned)rows[i].alloc, (unsigned)rows[i].cap,
                             (unsigned)rows[i].drop, (unsigned)rows[i].dropBytes);
    }
    QUEUE_HANDLE_TAG_LOG("Total allocated messages: %u", (unsigned)totalAllocated);
    if (registered > count) {
        /* N9: never report a silently truncated table - say how many rows were dropped and
         * what the cap is (the row buffer is intentionally small: this runs from the main
         * loop, not from a handler). */
        QUEUE_HANDLE_TAG_LOG("MessageQueueHandler: %u row(s) omitted (table cap = %u)",
                             (unsigned)(registered - count), (unsigned)(sizeof(rows) / sizeof(rows[0])));
    }
}

/* Read-only introspection over the registry (test hook / monitor use); the lock keeps a
 * concurrent bufferCreate()/end() from resizing the vector mid-read (MQ2). */
size_t messageQueueHandlerCount() {
    QUEUE_LOCK();
    const size_t n = _messageQueueHandlers.size();
    QUEUE_UNLOCK();
    return n;
}

ESPMessageQueue *messageQueueHandlerAt(size_t index) {
    QUEUE_LOCK();
    ESPMessageQueue *q = (index < _messageQueueHandlers.size()) ? _messageQueueHandlers[index] : nullptr;
    QUEUE_UNLOCK();
    return q;
}

/* Q2 / MQ16: human-readable cause for the caller-side logs. */
const char *mqSendResultToString(MqSendResult result) {
    switch (result) {
    case MQ_OK:       return "OK";
    case MQ_INVALID:  return "invalid";
    case MQ_NO_MEM:   return "no-mem";
    case MQ_FULL:     return "full";
    case MQ_NO_QUEUE: return "no-queue";
    default:          return "unknown";
    }
}

ESPMessageQueue::ESPMessageQueue()
    : _capacity(0), _queueHandler(nullptr), _counterAllocated(0), _dropped(0), _droppedBytes(0),
      _firstTimeStamp(0), _lastTimeStamp(0),
      _inFlightCount(0), _reservedInSend(0), _rxCount(0), _lastRxMs(0),
      _closing(false), _apiUsers(0), _pendingDelete(false), _deferRetries(0)
{
    for (size_t i = 0; i < QUEUE_IN_FLIGHT_MAX; ++i) { _inFlight[i] = nullptr; }
}

ESPMessageQueue::~ESPMessageQueue() {
    /* Q3 / MQ13 + N10: the destruction order of static objects against the module globals is
     * unspecified, so this path must not *depend* on them for correctness.  In practice it
     * still takes the module lock (cleanup() -> QUEUE_LOCK) and updates the global allocated
     * counter - both are safe because the lock is a static pointer to a heap mutex that is
     * never destroyed - but it never touches the handler registry: a destroyed queue would
     * leave a dangling row there.  N2: destruction is therefore only legal when no deferred
     * teardown is armed (teardownPending() == false); the firmware honours this by waiting
     * (see ESPFSEditor::editorListCtxMaybeFree) and never destroys a queue with a pending
     * teardown. */
    _destroyNoGlobal();
}

void ESPMessageQueue::_destroyNoGlobal() {
    if (_queueHandler) {
        cleanup();
        vQueueDelete(_queueHandler);
        _queueHandler = nullptr;
    }
    _capacity = 0;
    _counterAllocated = 0;
    _inFlightCount = 0;
    _reservedInSend = 0;
    _closing = false;
    _pendingDelete = false;
}

bool ESPMessageQueue::bufferCreate(int qSize, const char *elementName) {
    const char *name = (elementName != nullptr) ? elementName : "MessageQueue";

    if (qSize <= 0) {
        /* Q2 / MQ14: never report success for a queue that was not created. */
        QUEUE_HANDLE_TAG_ERROR("%s bufferCreate refused: qSize=%d", name, qSize);
        return false;
    }

    /* Q3 / MQ5: check-then-create under the module lock, so two tasks can never both
     * create (and leak) a queue for the same object.  The registry push is protected by
     * the same lock (MQ2). */
    QUEUE_LOCK();
    if (_queueHandler) {
        bool cancelledDelete = false;
        bool capacityMismatch = false;
        const size_t existingCapacity = _capacity;
        if (_closing) {
            /* Q3 / MQ1: a deferred delete is pending, but a new user wants this queue.
             * Cancel the teardown instead of handing out a queue that refuses every entry
             * (the armed ticker then sees _closing == false and cancels itself). */
            _closing = false;
            _pendingDelete = false;
            _deferRetries = 0;
            cancelledDelete = true;
        }
        capacityMismatch = ((size_t)qSize != _capacity);
        QUEUE_UNLOCK();
        if (cancelledDelete) {
            QUEUE_HANDLE_TAG_WARN("%s bufferCreate cancelled a pending deferred delete (queue reused)",
                                  _elementName.c_str());
            /* Q3 / MQ1 + N13: this is not a teardown-only path - it is also the reuse drain.
             * The aborted response may have left items behind and a fresh user must start
             * from an empty queue, otherwise the next listing would replay them. */
            cleanup();
        }
        if (capacityMismatch) {
            /* Q2 / MQ14: the queue already exists - the size argument is ignored on purpose
             * (the FS editor re-creates the same object per request).
             * N4: logged outside the module lock - a file log can block for hundreds of ms
             * and must never extend a critical section. */
            QUEUE_HANDLE_TAG_WARN("%s bufferCreate ignored: queue exists with capacity %u (asked %d)",
                                  _elementName.c_str(), (unsigned)existingCapacity, qSize);
        }
        return true;
    }

    _queueHandler = xQueueCreate(qSize, sizeof(messageHandle_t));
    if (!_queueHandler) {
        QUEUE_UNLOCK();
        QUEUE_HANDLE_TAG_ERROR("%s Queue init FAIL (qSize=%d)", name, qSize);
        return false;
    }
    _capacity = qSize;
    ++e_counter;
    _elementName = name;
    _elementName += "_";
    _elementName += e_counter;
    _messageQueueHandlers.push_back(this);
    QUEUE_UNLOCK();
    QUEUE_HANDLE_TAG_CONSOLE_I("%s Queue init OK", _elementName.c_str());
    return true;
}

MqSendResult ESPMessageQueue::bufferSend(const void *pvItem, size_t xItemSize, TickType_t xTicksToWait, void *arg) {
    if (!_apiEnter()) {
        /* Q2 / MQ16 + Q3 / MQ1: absent queue, or a teardown is in progress. */
        return MQ_NO_QUEUE;
    }
    if (!pvItem || 0 == xItemSize) {
        QUEUE_HANDLE_TAG_ERROR("%s bufferSend invalid argument (item=%p, size=%u)",
                               _elementName.c_str(), pvItem, (unsigned)xItemSize);
        _apiLeave();
        return MQ_INVALID;
    }

    uint8_t *buffer = new (std::nothrow) uint8_t[xItemSize];
    if (!buffer) {
        QUEUE_HANDLE_TAG_ERROR("%s allocation failure (%u bytes) - item dropped",
                               _elementName.c_str(), (unsigned)xItemSize);
        QUEUE_LOCK();
        ++_dropped;
        _droppedBytes += xItemSize;
        QUEUE_UNLOCK();
        _apiLeave();
        return MQ_NO_MEM;
    }

    /* Q3 / MQ3 + MQ4 + MQ10: counters and timestamps are updated under the module lock,
     * and "reserved for a send in progress" is tracked separately from "queued". */
    QUEUE_LOCK();
    if (isEmpty()) { _firstTimeStamp = millis(); }
    _lastTimeStamp = millis();
    ++_reservedInSend;
    ++queueCounterAllocated;
    QUEUE_UNLOCK();

    memcpy(buffer, pvItem, xItemSize);
    messageHandle_t handler;
    handler.pBuffer = (pMessage_t)buffer;
    handler.length = xItemSize;
    const bool status = xQueueSend(_queueHandler, &handler, xTicksToWait) == pdPASS;

    QUEUE_LOCK();
    if (_reservedInSend > 0) { --_reservedInSend; }
    if (!status) {
        if (queueCounterAllocated > 0) { --queueCounterAllocated; }
        delete[] buffer;
        ++_dropped;
        _droppedBytes += xItemSize;
    } else {
        ++_counterAllocated;
    }
    QUEUE_UNLOCK();

    if (!status) {
        QUEUE_HANDLE_TAG_WARN("%s send failed (full/timeout, %u bytes, wait=%u ticks) - item dropped",
                              _elementName.c_str(), (unsigned)xItemSize, (unsigned)xTicksToWait);
        _apiLeave();
        return MQ_FULL;
    }
    _apiLeave();
    return MQ_OK;
}

void* ESPMessageQueue::bufferReceive(size_t *pxItemSize, TickType_t xTicksToWait, void *arg) {
    /* Q2 / MQ11: leave a defined value on every exit path (timeout / no queue included). */
    if (pxItemSize) {
        *pxItemSize = 0;
    }
    if (!_apiEnter()) {
        return nullptr;
    }

    messageHandle_t handler;
    if (xQueueReceive(_queueHandler, &handler, xTicksToWait) == pdPASS) {
        QUEUE_LOCK();
        ++_rxCount;                       /* Q3 / MQ17 + MQ22: consumer progress metric */
        _lastRxMs = millis();
        bool tracked = false;
        if (_inFlightCount < QUEUE_IN_FLIGHT_MAX) {
            _inFlight[_inFlightCount++] = (void *)handler.pBuffer;   /* Q3 / MQ7 */
            tracked = true;
        }
        QUEUE_UNLOCK();
        if (!tracked) {
            /* N5: an item that cannot be tracked can never be validated on return - it would
             * be discarded (or freed) as an unknown pointer, i.e. an untracked leak.  Refuse
             * it instead: put it back (the queue is non-empty by construction, so the slot is
             * free right now) or, if even that fails, free it here and account the drop. */
            QUEUE_HANDLE_TAG_ERROR("%s in-flight set full (%u) - item %p refused",
                                   _elementName.c_str(), (unsigned)QUEUE_IN_FLIGHT_MAX, handler.pBuffer);
            if (xQueueSend(_queueHandler, &handler, 0) != pdPASS) {
                delete[] (uint8_t *)handler.pBuffer;
                QUEUE_LOCK();
                if (_counterAllocated > 0) { --_counterAllocated; }
                if (queueCounterAllocated > 0) { --queueCounterAllocated; }
                ++_dropped;
                _droppedBytes += handler.length;
                QUEUE_UNLOCK();
            }
            _apiLeave();
            return nullptr;
        }
        if (pxItemSize) {
            *pxItemSize = handler.length;
        }
        _apiLeave();
        return (void*)handler.pBuffer;
    }
    _apiLeave();
    return nullptr;
}

bool ESPMessageQueue::bufferReturnItem(void *pvItem, void *arg) {
    if (nullptr == pvItem) {
        return false;
    }
    if (!_apiEnter()) {
        return false;
    }

    size_t heldAfter = 0;
    bool found = false;
    QUEUE_LOCK();
    for (size_t i = 0; i < _inFlightCount; ++i) {
        if (_inFlight[i] == pvItem) {
            for (size_t j = i + 1; j < _inFlightCount; ++j) {
                _inFlight[j - 1] = _inFlight[j];
            }
            --_inFlightCount;
            _inFlight[_inFlightCount] = nullptr;
            found = true;
            break;
        }
    }
    heldAfter = _inFlightCount;
    QUEUE_UNLOCK();

    if (!found) {
        /* Q3 / MQ7: refuse pointers this queue never handed out (double return, foreign
         * pointer, an item already freed by cleanup(), or a tracking overflow).  The old
         * code reached delete[] with them - the heap layer then aborts (multi_heap_free). */
        QUEUE_HANDLE_TAG_ERROR("%s bufferReturnItem refused %p (not in flight, held=%u)",
                               _elementName.c_str(), pvItem, (unsigned)heldAfter);
        _apiLeave();
        return false;
    }

    delete[] (uint8_t *)pvItem;

    QUEUE_LOCK();
    if (queueCounterAllocated > 0) { --queueCounterAllocated; }
    if (_counterAllocated > 0) { --_counterAllocated; }
    QUEUE_UNLOCK();
    _apiLeave();
    return true;
}

/* Teardown drain: frees what is still queued AND what was handed out but never returned
 * (Q3 / MQ8 - a client disconnect used to leak the in-flight payloads forever).  Only
 * end()/_deleteNow()/_destroyNoGlobal() call it: on a live queue it would free blocks that
 * a receiver still holds. */
void ESPMessageQueue::cleanup() {
    QUEUE_LOCK();
    if (!_queueHandler) {
        QUEUE_UNLOCK();
        return;
    }

    uint32_t freedQueued = 0;
    uint32_t freedFlight = 0;
    messageHandle_t handler;
    while (xQueueReceive(_queueHandler, &handler, 0) == pdPASS) {
        if (handler.pBuffer) {
            delete[] (uint8_t *)handler.pBuffer;
            if (queueCounterAllocated > 0) { --queueCounterAllocated; }
            if (_counterAllocated > 0) { --_counterAllocated; }
            ++freedQueued;
        }
    }
    while (_inFlightCount > 0) {
        void *item = _inFlight[--_inFlightCount];
        _inFlight[_inFlightCount] = nullptr;
        if (item) {
            delete[] (uint8_t *)item;
            if (queueCounterAllocated > 0) { --queueCounterAllocated; }
            if (_counterAllocated > 0) { --_counterAllocated; }
            ++freedFlight;
        }
    }
    QUEUE_UNLOCK();

    if (freedQueued || freedFlight) {
        /* Q2 / MQ23 + Q3 / MQ8: teardown accounting (logged outside the lock). */
        QUEUE_HANDLE_TAG_WARN("%s cleanup freed %u queued + %u in-flight item(s)",
                              _elementName.c_str(), (unsigned)freedQueued, (unsigned)freedFlight);
    }
}

/* Q3 / MQ1: API entry gate.  false while the queue is absent or a teardown is pending, so
 * no new caller can enter the module between the last drain and the vQueueDelete. */
bool ESPMessageQueue::_apiEnter() {
    /* N8: fail closed.  If the module lock could not be created (OOM at static-init time)
     * the QUEUE_LOCK/QUEUE_UNLOCK macros above are no-ops, so every "protected" update
     * would silently run unsynchronised.  Without the lock the module refuses entry: the
     * caller gets MQ_NO_QUEUE / nullptr instead of a false sense of support. */
    if (_queueLock == nullptr) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            QUEUE_HANDLE_TAG_ERROR("module lock unavailable (SemaphoreHandle_t == NULL) - API refused");
        }
        return false;
    }
    QUEUE_LOCK();
    const bool ok = (_queueHandler != nullptr) && (!_closing);
    if (ok) { ++_apiUsers; }
    QUEUE_UNLOCK();
    return ok;
}

void ESPMessageQueue::_apiLeave() {
    QUEUE_LOCK();
    if (_apiUsers > 0) { --_apiUsers; }
    QUEUE_UNLOCK();
}

/* Physical teardown.  Callers guarantee that no API user is inside (or accept the
 * documented race of the destructor path, where nobody can hold a pointer any more). */
void ESPMessageQueue::_deleteNow() {
    cleanup();                       /* frees queued + in-flight items (MQ8) */
    QUEUE_LOCK();
    if (_queueHandler) {
        vQueueDelete(_queueHandler);
        _queueHandler = nullptr;
        _messageQueueHandlers.erase(std::remove(_messageQueueHandlers.begin(), _messageQueueHandlers.end(), this),
                                    _messageQueueHandlers.end());
    }
    /* Q3 / MQ8: reset every per-object value so a recreated queue starts clean.  `_apiUsers`
     * is reset too: the firmware may force-delete a task that was inside the module, which
     * would otherwise leave a reservation behind forever. */
    _capacity = 0;
    _counterAllocated = 0;
    _reservedInSend = 0;
    _firstTimeStamp = 0;
    _lastTimeStamp = 0;
    _rxCount = 0;                    /* N7: sinceLastRxMs() must report "no receive yet" */
    _lastRxMs = 0;
    _closing = false;
    _pendingDelete = false;
    _apiUsers = 0;
    _deferRetries = 0;
    QUEUE_UNLOCK();
    QUEUE_HANDLE_TAG_CONSOLE_I("%s Queue deleted", _elementName.c_str());
}

void ESPMessageQueue::_deferredDeleteTicker(void *arg) {
    if (arg) { static_cast<ESPMessageQueue *>(arg)->_deleteDeferred(); }
}

/* Q3 / MQ1: ticker side (runs in loopTask).  Re-arms itself while another task is still
 * inside the module instead of blocking; every API call is bounded (MQ20), so a retry
 * always terminates. */
void ESPMessageQueue::_deleteDeferred() {
    QUEUE_LOCK();
    const bool busy = (_apiUsers > 0);
    const bool alive = (_queueHandler != nullptr);
    const bool cancelled = (!_closing);   /* a new bufferCreate() re-opened the queue */
    QUEUE_UNLOCK();

    if ((!alive) || cancelled) {
        QUEUE_LOCK();
        _pendingDelete = false;
        _closing = false;
        _deferRetries = 0;
        QUEUE_UNLOCK();
        return;
    }
    /* Q3 / MQ1: delete deferred far enough that any live producer has left its bounded send.
     * `_apiUsers` is the primary gate, but it can only under-count: the firmware force-deletes
     * the chunked-response task (onDisconnect / the 5 s timeout ticker) even while it is
     * inside xQueueSend, in which case a reservation can never be released.  The retry count
     * therefore provides a hard cap (TASK_DELETE_DEFER_MAX, 6 s >= 2x the longest send wait). */
    if (busy && (_deferRetries < TASK_DELETE_DEFER_MAX)) {
        ++_deferRetries;
        if (!task_alloc_delete_ticker(TASK_DELETE_DEFER_INTERVAL_MS, &ESPMessageQueue::_deferredDeleteTicker, this)) {
            /* No ticker slot free: bounded wait, then delete safely or degrade (N3). */
            for (int i = 0; i < 300 && _apiUsers > 0; ++i) { vTaskDelay(1); }
            if (_apiUsers == 0) { _deleteNow(); } else { _abandonTeardown(); }
        }
        return;
    }
    if (busy) {
        /* N3: never vQueueDelete while a task is inside the module - deleting under a
         * blocked sender corrupts the queue's wait lists (the original MQ1 fault).  Degrade
         * to a permanently closed queue instead: every API call already fails with
         * MQ_NO_QUEUE, the control block is a few hundred bytes and the row stays visible in
         * the registry dump.  This is a small leak, never a safety issue. */
        _abandonTeardown();
        return;
    }
    _deleteNow();
}

/* N3: give up on the deferred teardown without deleting anything (the queue stays closed). */
void ESPMessageQueue::_abandonTeardown() {
    QUEUE_LOCK();
    _pendingDelete = false;      /* stop re-arming; _closing stays true -> the API stays refused */
    QUEUE_UNLOCK();
    QUEUE_HANDLE_TAG_ERROR("%s deferred delete ABANDONED (apiUsers>0 after %u retries) - queue stays closed",
                           _elementName.c_str(), (unsigned)TASK_DELETE_DEFER_MAX);
}

void ESPMessageQueue::end() {
    QUEUE_LOCK();
    if (!_queueHandler) {
        _closing = false;
        _pendingDelete = false;
        QUEUE_UNLOCK();
        return;
    }
    if (_pendingDelete) { QUEUE_UNLOCK(); return; }
    _closing = true;
    const bool idle = (_apiUsers == 0);
    if (!idle) { _pendingDelete = true; _deferRetries = 0; }
    QUEUE_UNLOCK();

    if (idle) {
        _deleteNow();
        return;
    }

    /* Q3 / MQ1: a task is still inside the module (the FS editor calls end() from async_tcp
     * while its producer task may still be in xQueueSend).  Never block async_tcp: defer
     * the vQueueDelete to a one-shot ticker callback in loopTask. */
    if (!task_alloc_delete_ticker(TASK_DELETE_DEFER_INTERVAL_MS, &ESPMessageQueue::_deferredDeleteTicker, this)) {
        /* N3: no ticker slot for the deferral -> bounded wait, then delete safely or degrade
         * to "closed forever"; never delete while an API user is inside. */
        for (int i = 0; i < 300 && _apiUsers > 0; ++i) { vTaskDelay(1); }
        if (_apiUsers == 0) { _deleteNow(); } else { _abandonTeardown(); }
    }
}

/* N2: read under the module lock - the caller may be a different task than the one that
 * armed the teardown (ticker/loopTask vs async_tcp), and an unsynchronised read could
 * report "not pending" while a ticker is already holding `this`. */
bool ESPMessageQueue::teardownPending() {
    QUEUE_LOCK();
    const bool pending = _pendingDelete;
    QUEUE_UNLOCK();
    return pending;
}

size_t ESPMessageQueue::liveTime() {
    /* Q2 / MQ15: no queue -> no live time (also keeps isEmpty() safe below). */
    if (!_queueHandler || isEmpty()) {
        return 0;
    }
    return millis() - _firstTimeStamp;
}

/* Q2 / MQ15: the accessors below are safe on an object whose bufferCreate() failed or that
 * was ended(): they report "nothing available" instead of dereferencing a NULL handle
 * (which used to abort through configASSERT inside the kernel). */
size_t ESPMessageQueue::spaceAvailable() {
    return _queueHandler ? uxQueueSpacesAvailable(_queueHandler) : 0;
}

size_t ESPMessageQueue::messageAvailable() {
    return _queueHandler ? uxQueueMessagesWaiting(_queueHandler) : 0;
}

size_t ESPMessageQueue::isEmpty() {
    return _queueHandler ? (uxQueueMessagesWaiting(_queueHandler) == 0) : 1;
}

size_t ESPMessageQueue::isFull() {
    return _queueHandler ? (uxQueueSpacesAvailable(_queueHandler) == 0) : 1;
}

/* Q3 accessors (read under the module lock so producer/consumer tasks see a stable value). */
size_t ESPMessageQueue::inFlight() {
    QUEUE_LOCK();
    const size_t v = _inFlightCount;
    QUEUE_UNLOCK();
    return v;
}

size_t ESPMessageQueue::reserved() {
    QUEUE_LOCK();
    const size_t v = _reservedInSend;
    QUEUE_UNLOCK();
    return v;
}

size_t ESPMessageQueue::drained() {
    QUEUE_LOCK();
    const size_t v = _rxCount;
    QUEUE_UNLOCK();
    return v;
}

uint32_t ESPMessageQueue::sinceLastRxMs() {
    QUEUE_LOCK();
    const uint32_t v = (_rxCount > 0) ? (millis() - _lastRxMs) : 0;
    QUEUE_UNLOCK();
    return v;
}
#endif // #if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 1)