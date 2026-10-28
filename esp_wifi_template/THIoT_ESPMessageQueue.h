#ifndef	__ESP_QUEUE_BUFFER_H
#define __ESP_QUEUE_BUFFER_H

#include <Arduino.h>
#include "THIoT_APPConfig.h"

#define QUEUE_BUFFER_DEBUG          0
#define MESSAGE_QUEUE_ALERT_NUM     64  // just for log recorder.

#if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 1)
void recordMessageQueueHandlers();
size_t messageQueueAllocated();
class ESPMessageQueue;  /* forward declaration (class is declared below) */
/* Read-only introspection over the queue registry (test hook / monitor use). */
size_t messageQueueHandlerCount();
ESPMessageQueue *messageQueueHandlerAt(size_t index);
/* Result of bufferSend().  MQ_OK == pdTRUE (=1) so existing `== pdTRUE` / bool tests keep
 * working; the other values let the caller report the real cause (Q2 / MQ16). */
typedef enum {
    MQ_OK = 1,      /* == pdTRUE: the item was copied and queued              */
    MQ_INVALID,     /* null item or zero length                               */
    MQ_NO_MEM,      /* the internal copy could not be allocated               */
    MQ_FULL,        /* queue did not accept the item within xTicksToWait      */
    MQ_NO_QUEUE     /* bufferCreate() was never called (or end() was)         */
} MqSendResult;

const char *mqSendResultToString(MqSendResult result);

#define QUEUE_IN_FLIGHT_MAX         8U                   /* items handed out but not returned (MQ7) */
#define QUEUE_API_DEFAULT_WAIT      pdMS_TO_TICKS(100)   /* MQ20: never block forever by default   */

/*
 * ESPMessageQueue contract (Q2/Q3: MQ6/MQ7/MQ8/MQ18/MQ19/MQ20/MQ21):
 * - Task context only.  Every method must run from a task: there is no ISR variant
 *   (xQueueSendFromISR / heap allocation must not run inside an ISR).
 * - bufferSend() deep-copies the payload into the queue; the caller keeps ownership of
 *   pvItem and may free it right after the call.  The default wait is bounded (MQ20).
 * - bufferReceive() transfers ownership of the returned block to the caller, who must
 *   release it with bufferReturnItem().  nullptr means "no item" and conflates empty /
 *   invalid queue / timeout - the three cases are NOT distinguished (MQ19).
 * - bufferReturnItem() VALIDATES the pointer against the in-flight set: a double return,
 *   a foreign pointer or an item of another queue is refused (false) and never freed
 *   (MQ7).  cleanup()/end() also free the items still in flight (MQ8).
 * - end() is safe to call while another task is inside the module: a `_closing` flag
 *   refuses new entries and the vQueueDelete is deferred to a ticker callback (loopTask)
 *   when an API user is still inside (MQ1).  Every call is bounded (MQ20), so the
 *   deferral always terminates.
 * - The optional `arg` parameter of the three transfer calls is unused/reserved (kept
 *   for symmetry with the xRingbuffer call sites); do not rely on it (MQ18).
 * - bufferCreate() returns false only when the queue could not be created (qSize <= 0 or
 *   xQueueCreate failure); an existing queue is kept and a size mismatch is warned (MQ14).
 * - Failure diagnostics go out through FS_TAG_LOGx (always compiled in); per-queue drop
 *   counters are readable via dropped()/droppedBytes() and the registry dump (MQ21).
 */
class ESPMessageQueue
{
private:
    xQueueHandle _queueHandler;
    String _elementName;
    size_t _capacity;
    size_t _counterAllocated;
    size_t _dropped;
    size_t _droppedBytes;
    uint32_t _firstTimeStamp;
    uint32_t _lastTimeStamp;

    /* Q3 */
    /* N6: in-flight entries are per-lifetime (no generation counter): a return that is refused
     * as "unknown pointer" is never freed, and after end()/bufferCreate() the set is empty
     * again - retrying the same old pointer would be refused again, so drop it instead. */
    void   *_inFlight[QUEUE_IN_FLIGHT_MAX];
    size_t  _inFlightCount;
    size_t  _reservedInSend;    /* allocated for a send in progress (MQ10) */
    size_t  _rxCount;           /* successful receives = consumer progress (MQ17/MQ22) */
    uint32_t _lastRxMs;
    bool    _closing;           /* teardown in progress: refuse new API entries (MQ1) */
    size_t  _apiUsers;          /* tasks currently inside the module (MQ1) */
    bool    _pendingDelete;     /* a deferred delete ticker is armed (MQ1) */
    uint16_t _deferRetries;     /* bounded retries of the deferred delete (>= longest send wait) */

    bool _apiEnter();           /* false when the queue is closing/absent */
    void _apiLeave();
    void _deleteNow();          /* cleanup + vQueueDelete + deregister + reset state */
    void _deleteDeferred();     /* ticker side of the deferred delete (MQ1) */
    void _abandonTeardown();    /* N3: give up without deleting (queue stays closed) */
    void _destroyNoGlobal();    /* destructor path: never touches module globals (MQ13) */
    static void _deferredDeleteTicker(void *arg);
public:
    ESPMessageQueue();
    ~ESPMessageQueue();
    void end();
    bool bufferCreate(int qSize, const char *elementName = "MessageQueue");
    MqSendResult bufferSend(const void *pvItem, size_t xItemSize, TickType_t xTicksToWait = QUEUE_API_DEFAULT_WAIT, void *arg = nullptr);
    void *bufferReceive(size_t *pxItemSize, TickType_t xTicksToWait = QUEUE_API_DEFAULT_WAIT, void *arg = nullptr);
    bool bufferReturnItem(void *pvItem, void *arg = nullptr);
    void cleanup();
    size_t liveTime();
    size_t spaceAvailable();
    size_t messageAvailable();
    size_t isEmpty();
    size_t isFull();
    size_t capacity() {
        return _capacity;
    }
    bool isValid() {
        return _queueHandler != nullptr;
    }
    size_t allocated() {
        return _counterAllocated;
    }
    size_t dropped() {
        return _dropped;
    }
    size_t droppedBytes() {
        return _droppedBytes;
    }
    size_t inFlight();          /* items handed out and not yet returned (MQ7/MQ8) */
    size_t reserved();          /* allocated for a send in progress (MQ10)          */
    size_t drained();           /* successful receives = consumer progress          */
    uint32_t sinceLastRxMs();   /* ms since the last successful receive (0 if none) */
    /* N2: true while a deferred teardown is armed (a ticker holds `this`).  A caller that
     * owns the object must not destroy it before this is false.  Defined in the .cpp so the
     * read is taken under the module lock (cross-task caller: the ticker runs in loopTask). */
    bool teardownPending();
    /* Q2 / MQ12: by reference - no String copy on every registry dump. */
    const String &elementName() {
        return _elementName;
    }
};

#else
/* Q4 / MQ24: the ESPMessageQueue substitution is no longer switchable.  The call sites in
 * I2CData.cpp, web485_v3.ino and THIoT_APPParamSettingLogHandler.h were written against this class and
 * were never guarded, so MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 0 does not build anymore.
 * Flipping the flag now stops with this message instead of failing deep in a caller.
 * See docs/ESPMESSAGEQUEUE_FIX_PLAN.md (decision D5). */
#error "MESSAGE_QUEUE_REPLACE_X_RING_BUFFER=0 is unsupported in this firmware - see THIoT_ESPMessageQueue.h"
#endif // #if (MESSAGE_QUEUE_REPLACE_X_RING_BUFFER == 1)
#endif // __ESP_QUEUE_BUFFER_H
