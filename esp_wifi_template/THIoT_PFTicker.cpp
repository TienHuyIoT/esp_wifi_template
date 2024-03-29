#include <stdlib.h>
#include <stdint.h>
#include <Arduino.h>
#include "THIoT_PFcList.h"
#include "THIoT_PFTicker.h"
#include "THIoT_APPConfig.h"

#if (MUTEX_PLATFORM_TICKER == 1)
SemaphoreHandle_t mutex_ticker;
#else
#warning Should be enable MUTEX_PLATFORM_TICKER
#endif
static uint8_t function_counter = 0;
static c_list_t* list_fp_handler = nullptr;
static void IRAM_ATTR _schedule_handler(ticker_function_handle_t *fp_ticker);

static uint8_t _attach_ms(ticker_function_handle_t *fp_ticker, 
                        fp_callback fp_register,
                        void* arg, 
                        uint32_t interval, 
                        uint16_t repeat);

void ticker_begin(ticker_t *ticker, uint32_t interval) {
	ticker->start = millis();
	ticker->interval = interval;
	ticker->status = TICKER_RUNNING;
}

/** 
 * @brief trigger a timeout without change interval
 * start + interval - millis() = timeout
*/
void ticker_trigger(ticker_t *ticker, uint32_t timeout) {
  ticker->start = timeout + millis() - ticker->interval;
}

/* brief: check ticker expired
 * return 1: ticker running to stop
 * return 0: ticker stop
 * */
uint8_t ticker_expired(ticker_t *ticker) {
	if((TICKER_RUNNING == ticker->status) && ((millis() - ticker->start) >= ticker->interval)) {
		ticker->status = TICKER_STOP;
		return 1;
	}
	return 0;
}

void ticker_stop(ticker_t *ticker) {
	ticker->status = TICKER_STOP;
}

ticker_status_t ticker_status(ticker_t *ticker) {
	return ticker->status;
}

uint32_t ticker_remain(ticker_t *ticker) {
	if(TICKER_STOP == ticker->status) return 0;
	if((millis() - ticker->start) >= ticker->interval) return 0;
	return (ticker->start + ticker->interval - millis());
}

void ticker_refresh(ticker_t *ticker) {
	ticker->start = millis();
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
uint8_t ticker_attach_trigger(ticker_function_handle_t *fp_ticker,
                        uint32_t timeout)
{
  ticker_trigger(&fp_ticker->ticker, timeout * 1000);
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
  return _attach_ms(fp_ticker, fp_register, arg, interval * 1000, 1);
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
  return _attach_ms(fp_ticker, fp_register, arg, interval, 1);
}

/** 
 * @brief Ticker function register init.
 * @param [in] fp_register The function callback register pointer.
 * @param [in] interval The millisecond timeout interval.
 * @return 0 (error), otherwise return index function register.
*/
uint8_t allocate_ticker_once_ms(uint32_t interval,
                        fp_callback fp_register,
                        void* arg)
{
  ticker_function_handle_t *fp_ticker = (ticker_function_handle_t*)malloc(sizeof(ticker_function_handle_t));
  if (NULL == fp_ticker) {
    return 0;
  }
  fp_ticker->heap_flag = LIST_DYNAMIC_VALUE;
  return _attach_ms(fp_ticker, fp_register, arg, interval, 1);
}

void ticker_detach(ticker_function_handle_t *fp_ticker)
{
#if (MUTEX_PLATFORM_TICKER == 1)
  if(xSemaphoreTake(mutex_ticker, portMAX_DELAY) == pdTRUE) 
#endif
  {
    if (c_list_removeByValue(&list_fp_handler, fp_ticker, fp_ticker->heap_flag) == LIST_OK)
    {
      ticker_stop(&fp_ticker->ticker); 
      function_counter--;
    }
#if (MUTEX_PLATFORM_TICKER == 1)
    xSemaphoreGive(mutex_ticker);
#endif
  }
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
  uint8_t result = 0;
  ticker_begin(&fp_ticker->ticker, interval);
  fp_ticker->cb = fp_register;
  fp_ticker->arg = arg;
  fp_ticker->repeat = repeat;
#if (MUTEX_PLATFORM_TICKER == 1)
  if(xSemaphoreTake(mutex_ticker, portMAX_DELAY) == pdTRUE) 
#endif
  {
    if (c_list_findByValue(&list_fp_handler, fp_ticker) == NULL)
    {
      if(c_list_insert(&list_fp_handler, fp_ticker))
      {
        function_counter++;
        result = 1;
      }
    }
#if (MUTEX_PLATFORM_TICKER == 1)
    xSemaphoreGive(mutex_ticker);
#endif
  }

  return result;
}

#if (MUTEX_PLATFORM_TICKER == 1)
void ticker_schedule_init(void) {
  mutex_ticker = xSemaphoreCreateMutex();
}
#endif

void IRAM_ATTR ticker_schedule_handler(void)
{
    c_list_t *pList = list_fp_handler;
    ticker_function_handle_t *fp_handler;
    while (pList) {
#if (MUTEX_PLATFORM_TICKER == 1)
      if(xSemaphoreTake(mutex_ticker, portMAX_DELAY) == pdTRUE) {
#endif
        if (c_list_validate(&list_fp_handler, pList) == LIST_ERROR) {
          return;
        }
        fp_handler = (ticker_function_handle_t*)c_list_get_value(pList);
        pList = c_list_next(pList);
#if (MUTEX_PLATFORM_TICKER == 1)
        xSemaphoreGive(mutex_ticker);
      }
#endif
      /** The schedule should be outside the mutex lock
       * to avoid dead lock when the callback procedures has a call the ticker_detach()
       * or inside the schedule has a call ticker_detach()
      */
      _schedule_handler(fp_handler);
    }
}

static void IRAM_ATTR _schedule_handler(ticker_function_handle_t *fp_ticker)
{
  if (NULL == fp_ticker) {
    return;
  }

  if (!ticker_expired(&fp_ticker->ticker)) {
    return;
  }

  fp_ticker->ticker.start += fp_ticker->ticker.interval;

  if(TICKER_FOREVER == fp_ticker->repeat) {
    fp_ticker->ticker.status = TICKER_RUNNING;
  }
  else {
    --fp_ticker->repeat;
    if(fp_ticker->repeat) {
      fp_ticker->ticker.status = TICKER_RUNNING;
    }
    else {
      ticker_detach(fp_ticker);
    }
  }

  fp_ticker->cb(fp_ticker->arg);
}
