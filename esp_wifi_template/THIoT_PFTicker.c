#include <stdlib.h>
#include <stdint.h>
#include "THIoT_PFcList.h"
#include "THIoT_PFTicker.h"

extern unsigned long millis();

static uint8_t function_counter = 0;
static list_t* list_fp_handler = NULL;
static void _schedule_handler(ticker_function_handle_t *fp_ticker);

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

/* brief: check ticker expried
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
 * @brief Ticker function register init.
 * @param [in] fp_ticker The ticker function object.
 * @param [in] fp_register The function callback register pointer.
 * @param [in] interval The millisecond timeout interval.
 * @return 0 (error), otherwise return index function register.
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
  ticker_function_handle_t *fp_ticker = malloc(sizeof(ticker_function_handle_t));
  if (NULL == fp_ticker)
  {
    return 0;
  }
  fp_ticker->heap_flag = LIST_DYNAMIC_VALUE;
  return _attach_ms(fp_ticker, fp_register, arg, interval, 1);
}

void ticker_detach(ticker_function_handle_t *fp_ticker)
{
  if (list_removeByValue(&list_fp_handler, fp_ticker, fp_ticker->heap_flag) == LIST_OK)
  {
    ticker_stop(&fp_ticker->ticker); 
    function_counter--;
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

  ticker_begin(&fp_ticker->ticker, interval);
  fp_ticker->cb = fp_register;
  fp_ticker->arg = arg;
  fp_ticker->repeat = repeat;

  if (list_findByValue(&list_fp_handler, fp_ticker) == NULL)
  {
    if(list_insert(&list_fp_handler, fp_ticker))
    {
      function_counter++;
      // insert list succeed
      return 1;
    }
  }

  // insert list failed
  return 0;
}

void ticker_schedule_handler(void)
{
  list_t *pList = list_fp_handler;
  ticker_function_handle_t *fp_handler;
  while (pList)
  {
    fp_handler = list_get_value(pList);
    _schedule_handler(fp_handler);
    pList = list_next(pList);
  }
}

static void _schedule_handler(ticker_function_handle_t *fp_ticker)
{
  if (!ticker_expired(&fp_ticker->ticker)) {
    return;
  }

  // update offset timer start
  fp_ticker->ticker.start += fp_ticker->ticker.interval;

  if(TICKER_FOREVER == fp_ticker->repeat)
  {
    fp_ticker->ticker.status = TICKER_RUNNING;
  }
  else
  {
    --fp_ticker->repeat;
    if(fp_ticker->repeat)
    {
      fp_ticker->ticker.status = TICKER_RUNNING;
    }
    else
    {
      ticker_detach(fp_ticker);
    }
  }
  // call cb function
  fp_ticker->cb(fp_ticker->arg);
}
