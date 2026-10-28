#include <stdlib.h>
#include <stdint.h>
#include <Arduino.h>
#include "THIoT_PFcList.h"
#include "THIoT_PFTicker.h"
#include "THIoT_APPConfig.h"
#include "THIoT_PFSerialTrace.h"

// P16: Diagnostics for ERROR paths only.
// DO NOT enable TICKER_TAG_CONSOLE_I: tickers re-arm at high frequency, so a
// per-call INFO log floods the console-trace pipeline (ring-buffer send with
// portMAX_DELAY + per-record String + file rotation via backup2Folder), which
// drains/fragments the heap and hard-resets the device. Keep INFO disabled.
#define TICKER_TAG_CONSOLE_I(...)       //CONSOLE_TAG_LOGI("[TICKER]", __VA_ARGS__)
#define TICKER_TAG_CONSOLE_E(...)       CONSOLE_TAG_LOGE("[TICKER]", __VA_ARGS__)

#if (defined MUTEX_PLATFORM_TICKER) && (MUTEX_PLATFORM_TICKER == 1)
#define _ticker_semaphore_take(x)   do{if ((x)->xSemaphore) {xSemaphoreTakeRecursive((x)->xSemaphore, portMAX_DELAY);}}while(0)
#define _ticker_semaphore_give(x)   do{if ((x)->xSemaphore) {xSemaphoreGiveRecursive((x)->xSemaphore);}}while(0)
#define _ticker_semaphore_delete(x) do{if ((x)->xSemaphore) {vSemaphoreDelete((x)->xSemaphore); (x)->xSemaphore = nullptr;}}while(0)
#define _ticker_semaphore_init(x)   do{if (!((x)->xSemaphore)) {(x)->xSemaphore = xSemaphoreCreateRecursiveMutex();}}while(0)
#define _ticker_semaphore_isnull_asset(x)  (!((x)->xSemaphore))

#define _list_semaphore_take(x)     do{if (x) {xSemaphoreTake(x, portMAX_DELAY);}}while(0)
#define _list_semaphore_give(x)     do{if (x) {xSemaphoreGive(x);}}while(0)
#define _list_semaphore_delete(x)   do{if (x) {vSemaphoreDelete(x); (x) = nullptr;}}while(0)
#define _list_semaphore_init(x)     do{if (!(x)) {x = xSemaphoreCreateMutex();}}while(0)
#define _list_semaphore_isnull_asset(x)   (!(x))
static SemaphoreHandle_t mutex_list_ticker = nullptr;
#else
#define mutex_list_ticker                   0
#define _ticker_semaphore_take(x)
#define _ticker_semaphore_give(x)
#define _ticker_semaphore_delete(x)
#define _ticker_semaphore_init(x)
#define _ticker_semaphore_isnull_asset(x)   0

#define _list_semaphore_take(x)
#define _list_semaphore_give(x)
#define _list_semaphore_delete(x)
#define _list_semaphore_init(x)
#define _list_semaphore_isnull_asset(x)     0
#ifdef ESP32
#warning Should be enable MUTEX_PLATFORM_TICKER
#endif
#endif
static uint8_t function_counter = 0;
static c_list_t* list_fp_handler = nullptr;
static void IRAM_ATTR _schedule_handler(ticker_function_handle_t *fp_ticker);

#if (defined MUTEX_PLATFORM_TICKER) && (MUTEX_PLATFORM_TICKER == 1)
iot_ticker::iot_ticker(uint32_t timeout, bool run) {
  start = millis();
  interval = timeout;
  xSemaphore = xSemaphoreCreateRecursiveMutex();
  if (run) {
    status = TICKER_RUNNING;
  } else {
    status = TICKER_STOP;
  }
}

iot_ticker::~iot_ticker() {
  if (xSemaphore) {
    TICKER_TAG_CONSOLE_I("Delete semaphore");
    vSemaphoreDelete(xSemaphore);
    xSemaphore = nullptr;
  }
}
#endif

static uint8_t _attach_ms(ticker_function_handle_t *fp_ticker, 
                        fp_callback fp_register,
                        void* arg, 
                        uint32_t interval, 
                        uint16_t repeat);

void ticker_begin(ticker_t *ticker, uint32_t interval) {
  _ticker_semaphore_init(ticker);
  _ticker_semaphore_take(ticker);
	ticker->start = millis();
	ticker->interval = interval;
	ticker->status = TICKER_RUNNING;
  _ticker_semaphore_give(ticker);
}

/* brief: check ticker expired
 * return 1: ticker running to stop
 * return 0: ticker stop
 * */
boolean ticker_expired(ticker_t *ticker) {
  uint32_t timeBoot = millis();
  boolean status = false;
  _ticker_semaphore_init(ticker);
  _ticker_semaphore_take(ticker);
  if ((TICKER_RUNNING == ticker->status)) {
    int interval = ticker->interval;
    int timeout = timeBoot - ticker->start;
    if (timeout >= interval) {
      ticker->status = TICKER_STOP;
      status = true;
    }
  }
  _ticker_semaphore_give(ticker);
  return status;
}

/** 
 * @brief trigger a timeout without change interval by re-update time start
 * start + interval - millis() = timeout
 * 
 * @param in timeout: it can be higher than interval
*/
void ticker_trigger(ticker_t *ticker, uint32_t timeout) {
  _ticker_semaphore_init(ticker);
  _ticker_semaphore_take(ticker);
  ticker->start = millis() + timeout - ticker->interval;
  _ticker_semaphore_give(ticker);
}

void ticker_stop(ticker_t *ticker) {
  _ticker_semaphore_init(ticker);
  _ticker_semaphore_take(ticker);
	ticker->status = TICKER_STOP;
  _ticker_semaphore_give(ticker);
}

ticker_status_t ticker_status(ticker_t *ticker) {
  ticker_expired(ticker);

  ticker_status_t status;
  _ticker_semaphore_init(ticker);
  _ticker_semaphore_take(ticker);
	status = ticker->status;
  _ticker_semaphore_give(ticker);
	return status;
}

uint32_t ticker_remain(ticker_t *ticker) {
  uint32_t remain = 0;
  unsigned long timeBoot = millis();
  _ticker_semaphore_init(ticker);
  _ticker_semaphore_take(ticker);
	if(TICKER_RUNNING == ticker->status) {
    if((timeBoot - ticker->start) < ticker->interval) {     
      remain = (ticker->start + ticker->interval - timeBoot);
    }
  }
  _ticker_semaphore_give(ticker);
  return remain;
}

void ticker_refresh(ticker_t *ticker) {
  _ticker_semaphore_init(ticker);
  _ticker_semaphore_take(ticker);
	ticker->start = millis();
  _ticker_semaphore_give(ticker);
}

void ticker_restart(ticker_t *ticker) {
  _ticker_semaphore_init(ticker);
  _ticker_semaphore_take(ticker);
	ticker->start = millis();
	ticker->status = TICKER_RUNNING;
  _ticker_semaphore_give(ticker);
}

void ticker_update(ticker_t *ticker, uint32_t interval) {
	ticker_begin(ticker, interval);
}

/** 
 * @brief Ticker function register init.
 * @param [in] fp_ticker The ticker function object.
 * @param [in] fp_register The function callback register pointer.
 * @param [in] interval The second timeout interval.
 * @return 0 (error), otherwise return index function register.
 * 
 * Note: Do not need to be detach any more before call this function
*/
uint8_t ticker_attach(ticker_function_handle_t *fp_ticker, 
                        uint32_t interval,
                        fp_callback fp_register,
                        void* arg)
{
  fp_ticker->heap_flag = LIST_STATIC_VALUE;
  return _attach_ms(fp_ticker, fp_register, arg, interval * 1000, TICKER_FOREVER);
}

/** 
 * @brief trigger a timeout without change interval
*/
void ticker_attach_trigger(ticker_function_handle_t *fp_ticker, uint32_t timeout)
{
  ticker_trigger(&fp_ticker->ticker, timeout * 1000);
}

void ticker_attach_ms_trigger(ticker_function_handle_t *fp_ticker, uint32_t timeout)
{
  ticker_trigger(&fp_ticker->ticker, timeout);
}

/** 
 * @brief Ticker function register init.
 * @param [in] fp_ticker The ticker function object.
 * @param [in] fp_register The function callback register pointer.
 * @param [in] interval The millisecond timeout interval.
 * @return 0 (error), otherwise return index function register.
 * 
 * Note: Do not need to be detach any more before call this function
*/
uint8_t ticker_attach_ms(ticker_function_handle_t *fp_ticker, 
                        uint32_t interval,
                        fp_callback fp_register,
                        void* arg)
{
  fp_ticker->heap_flag = LIST_STATIC_VALUE;
  return _attach_ms(fp_ticker, fp_register, arg, interval, TICKER_FOREVER);
}

/** 
 * @brief Ticker function register init.
 * @param [in] fp_ticker The ticker function object.
 * @param [in] fp_register The function callback register pointer.
 * @param [in] interval The second timeout interval.
 * @return 0 (error), otherwise return index function register.
 * 
 * Note: Do not need to be detach any more before call this function
*/
uint8_t ticker_once(ticker_function_handle_t *fp_ticker, 
                        uint32_t interval,
                        fp_callback fp_register,
                        void* arg)
{
  fp_ticker->heap_flag = LIST_STATIC_VALUE;
  return _attach_ms(fp_ticker, fp_register, arg, interval * 1000, TICKER_ONCE);
}

/** 
 * @brief Ticker function register init.
 * @param [in] fp_ticker The ticker function object.
 * @param [in] fp_register The function callback register pointer.
 * @param [in] interval The millisecond timeout interval.
 * @return 0 (error), otherwise return index function register.
 * 
 * Note: Do not need to be detach any more before call this function
*/
uint8_t ticker_once_ms(ticker_function_handle_t *fp_ticker, 
                        uint32_t interval,
                        fp_callback fp_register,
                        void* arg)
{
  fp_ticker->heap_flag = LIST_STATIC_VALUE;
  return _attach_ms(fp_ticker, fp_register, arg, interval, TICKER_ONCE);
}

/** 
 * @brief Ticker function register init.
 * @param [in] fp_register The function callback register pointer.
 * @param [in] interval The millisecond timeout interval.
 * @return pointer of the function ticker allocated, or nullptr on failure.
 * 
 * P10: Must check result and return nullptr on allocation/registration failure.
*/
ticker_function_handle_t *allocate_ticker_once_ms(uint32_t interval,
                                                  fp_callback fp_register,
                                                  void *arg)
{
  // ticker_function_handle_t *fp_ticker = (ticker_function_handle_t*)malloc(sizeof(ticker_function_handle_t));
  ticker_function_handle_t *fp_ticker = new (std::nothrow) ticker_function_handle_t;
  if (!fp_ticker) {
    TICKER_TAG_CONSOLE_E("allocate_ticker_once_ms: malloc failed");
    return nullptr;
  }
  TICKER_TAG_CONSOLE_I("Allocated fp %u", fp_ticker);
  fp_ticker->heap_flag = LIST_DYNAMIC_VALUE;
  
  // P10: Check _attach_ms result — fail if registry insertion fails
  uint8_t attach_result = _attach_ms(fp_ticker, fp_register, arg, interval, TICKER_ONCE);
  if (!attach_result) {
    TICKER_TAG_CONSOLE_E("allocate_ticker_once_ms: _attach_ms failed (registry full or malloc error)");
    delete fp_ticker;
    return nullptr;
  }
  return fp_ticker;
}

void ticker_detach(ticker_function_handle_t *fp_ticker)
{
  if (!fp_ticker) {
    return;
  }

#if (1)
  _list_semaphore_take(mutex_list_ticker);
  if (c_list_findByValue(&list_fp_handler, fp_ticker)) {
    if (fp_ticker->state != TICKER_FUNCTION_REJECT) {
      fp_ticker->state = TICKER_FUNCTION_REJECT;

      // stop ticker
      _ticker_semaphore_init(&fp_ticker->ticker);
      _ticker_semaphore_take(&fp_ticker->ticker);
      fp_ticker->ticker.status = TICKER_STOP;
      _ticker_semaphore_give(&fp_ticker->ticker);
    }
  }
  _list_semaphore_give(mutex_list_ticker);
#else
  _list_semaphore_take(mutex_list_ticker);
  c_list_t *pList = c_list_findByValue(&list_fp_handler, fp_ticker);
  if (pList != NULL)
  {
    uint8_t status = c_list_remove(&list_fp_handler, pList, LIST_STATIC_VALUE);
    if (LIST_OK == status)
    {
      // stop ticker
      _ticker_semaphore_init(&fp_ticker->ticker);
      _ticker_semaphore_take(&fp_ticker->ticker);
      fp_ticker->ticker.status = TICKER_STOP;
      fp_ticker->state = TICKER_FUNCTION_REJECT;
      _ticker_semaphore_give(&fp_ticker->ticker);
      // Check dynamic ticker and free memory
      if (LIST_DYNAMIC_VALUE == fp_ticker->heap_flag) {
        delete fp_ticker;
      }
      function_counter--;
    }
  }
  _list_semaphore_give(mutex_list_ticker);
#endif
}

/** 
 * @brief Ticker function register init.
 * @param [in] fp_ticker The ticker function object.
 * @param [in] fp_register The function callback register pointer.
 * @param [in] interval The timeout interval.
 * @param [in] repeat The counter call function callback.
 *                    macro TICKER_FOREVER will repeat forever.
 * @return 0 (error), otherwise return index function register.
*/
static uint8_t _attach_ms(ticker_function_handle_t *fp_ticker, 
                        fp_callback fp_register,
                        void* arg, 
                        uint32_t interval, 
                        uint16_t repeat)
{
  // P16: NULL-check handle
  if (!fp_ticker) {
    TICKER_TAG_CONSOLE_E("_attach_ms: null handle");
    return 0;
  }
  
  uint8_t result = 0;
  _ticker_semaphore_init(&fp_ticker->ticker);
  
  // P16: assert on mutex creation failure
  if (!fp_ticker->ticker.xSemaphore) {
    TICKER_TAG_CONSOLE_E("_attach_ms: mutex creation failed");
    return 0;
  }
  
  _ticker_semaphore_take(&fp_ticker->ticker);
  // Init ticker
  fp_ticker->ticker.start = millis();
	fp_ticker->ticker.interval = interval;
	fp_ticker->ticker.status = TICKER_RUNNING;
  // init parameters
  fp_ticker->cb = fp_register;
  fp_ticker->arg = arg;
  fp_ticker->repeat = repeat;
  _ticker_semaphore_give(&fp_ticker->ticker);

  _list_semaphore_take(mutex_list_ticker);
  if (c_list_findByValue(&list_fp_handler, fp_ticker) == NULL)
  {
    if(c_list_insert(&list_fp_handler, fp_ticker))
    {
      fp_ticker->state = TICKER_FUNCTION_INSERTED;
      function_counter++;
      result = 1;
      TICKER_TAG_CONSOLE_I("Registered handle, count=%u", function_counter);
    }
    else
    {
      TICKER_TAG_CONSOLE_E("_attach_ms: registry insert failed (malloc?)");
    }
  } else {
      fp_ticker->state = TICKER_FUNCTION_INSERTED;
      TICKER_TAG_CONSOLE_I("Re-armed handle");
  }
  _list_semaphore_give(mutex_list_ticker);

  return result;
}

#if (defined MUTEX_PLATFORM_TICKER) && (MUTEX_PLATFORM_TICKER == 1)
void ticker_schedule_init(void) {
  _list_semaphore_init(mutex_list_ticker);
}
#endif

/**
 * @brief Scheduler dispatcher — must be called ONLY from loopTask
 * 
 * IMPORTANT (P6): This function processes all ticker callbacks. 
 * It is designed to run as a single scheduler from loopTask only.
 * Calling from multiple tasks concurrently will cause:
 * - Race conditions on registry iteration
 * - Double-execution of callbacks
 * - Potential deadlock/corruption
 * 
 * Do not call this function from other tasks.
*/
void ticker_schedule_handler(void)
{
  ticker_function_handle_t *fp_handler;
  
  // P5: Take registry mutex before first read to avoid UB
  _list_semaphore_take(mutex_list_ticker);
  c_list_t *pList = list_fp_handler;
  _list_semaphore_give(mutex_list_ticker);
  
  while (pList) {
    _list_semaphore_take(mutex_list_ticker);
    if (c_list_validate(&list_fp_handler, pList) == LIST_ERROR) {
      _list_semaphore_give(mutex_list_ticker);
      return;
    }

    fp_handler = (ticker_function_handle_t*)c_list_get_value(pList);
    if (fp_handler && fp_handler->state == TICKER_FUNCTION_REJECT) {
      c_list_t *next = c_list_next(pList);  // P11: capture next before remove
      c_list_removeByValue(&list_fp_handler, fp_handler, LIST_STATIC_VALUE);
      fp_handler->state = TICKER_FUNCTION_FREE;
      if (LIST_DYNAMIC_VALUE == fp_handler->heap_flag) {
        TICKER_TAG_CONSOLE_I("Delete fp %u", fp_handler);
        delete fp_handler;
      }
      function_counter--;
      fp_handler = nullptr;
      pList = next;  // P11: use captured next, not freed memory
    } else {
      pList = c_list_next(pList);
    }
    _list_semaphore_give(mutex_list_ticker);

    /** The schedule should be outside the mutex lock
     * to avoid dead lock when the callback procedures has a call the ticker_detach()
     * or inside the schedule has a call ticker_detach()
     * 
     * CORRECTED (P4): Recursive mutex alone does NOT prevent cross-task inversion.
     * See P2 for lock-order constraints and P3 for safer callback execution model.
    */
    _schedule_handler(fp_handler);
  }
}

static void IRAM_ATTR _schedule_handler(ticker_function_handle_t *fp_ticker)
{
  if (!fp_ticker) {
    return;
  }

#if (defined MUTEX_PLATFORM_TICKER) && (MUTEX_PLATFORM_TICKER == 1)
  if (!_ticker_semaphore_isnull_asset(&fp_ticker->ticker)) {
    _ticker_semaphore_take(&fp_ticker->ticker);

    if (!ticker_expired(&fp_ticker->ticker))
    {
      _ticker_semaphore_give(&fp_ticker->ticker);
      return;
    }

    ticker_status_t status = TICKER_STOP;
    if (TICKER_FOREVER == fp_ticker->repeat)
    {
      status = TICKER_RUNNING;
    }
    else
    {
      --fp_ticker->repeat;
      if (fp_ticker->repeat)
      {
        status = TICKER_RUNNING;
      }
      else
      {
        ticker_detach(fp_ticker);
      }
    }

    fp_ticker->ticker.start += fp_ticker->ticker.interval;
    fp_ticker->ticker.status = status;

    // P17: Wrap callback in try/catch — exceptions cause portMAX_DELAY locks
    if (fp_ticker->cb)
    {
      try {
        fp_ticker->cb(fp_ticker->arg);
      }
      catch (const std::exception& e) {
        TICKER_TAG_CONSOLE_E("_schedule_handler: callback threw exception: %s", e.what());
      }
      catch (...) {
        TICKER_TAG_CONSOLE_E("_schedule_handler: callback threw unknown exception");
      }
    }
    _ticker_semaphore_give(&fp_ticker->ticker);
  }
#else
  if (!ticker_expired(&fp_ticker->ticker))
  {
    return;
  }

  ticker_status_t status = TICKER_STOP;
  if (TICKER_FOREVER == fp_ticker->repeat)
  {
    status = TICKER_RUNNING;
  }
  else
  {
    --fp_ticker->repeat;
    if (fp_ticker->repeat)
    {
      status = TICKER_RUNNING;
    }
    else
    {
      ticker_detach(fp_ticker);
    }
  }
  fp_ticker->ticker.start += fp_ticker->ticker.interval;
  fp_ticker->ticker.status = status;

  if (fp_ticker->cb) {
    try {
      fp_ticker->cb(fp_ticker->arg);
    }
    catch (const std::exception& e) {
      TICKER_TAG_CONSOLE_E("_schedule_handler: callback threw exception: %s", e.what());
    }
    catch (...) {
      TICKER_TAG_CONSOLE_E("_schedule_handler: callback threw unknown exception");
    }
  }
#endif
}
