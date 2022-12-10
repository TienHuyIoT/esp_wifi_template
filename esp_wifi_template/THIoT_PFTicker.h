#ifndef _TICKER_H_
#define _TICKER_H_

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

#define TICKER_FUNCTION_HANDLE_MAX  20	/* maximum number ticker function handler */
#define TICKER_FOREVER              0
#define TICKER_START_INIT  {\
                              .start = 0,\
                              .interval = 0,\
                              .status = TICKER_RUNNING\
                           }

#define TICKER_STOP_INIT   {\
                              .start = 0,\
                              .interval = 0,\
                              .status = TICKER_STOP\
                           }

typedef enum
{
	TICKER_STOP = 0,
	TICKER_RUNNING
}ticker_status_t;

typedef struct{
	uint32_t 	start;   //ticker Start
	uint32_t 	interval;   //tickerout_Value
	ticker_status_t 	status;
}ticker_t;

typedef void (*fp_callback)(void*);

typedef struct ticker_function_handle
{
  ticker_t ticker;
  fp_callback cb;
  void *arg;
  uint16_t repeat;
  uint8_t  heap_flag;
} ticker_function_handle_t;

uint8_t ticker_expired(ticker_t *ticker);
void ticker_begin(ticker_t *ticker, uint32_t interval);
void ticker_stop(ticker_t *ticker);
ticker_status_t ticker_status(ticker_t *ticker);
uint32_t ticker_remain(ticker_t *ticker);
void ticker_refresh(ticker_t *ticker);
void ticker_update(ticker_t *ticker, uint32_t interval);

uint8_t ticker_attach(ticker_function_handle_t *fp_ticker,
                        uint32_t interval,
                        fp_callback fp_register,
                        void* arg);

uint8_t ticker_attach_ms(ticker_function_handle_t *fp_ticker,
                        uint32_t interval,
                        fp_callback fp_register,
                        void* arg);

uint8_t ticker_once(ticker_function_handle_t *fp_ticker,
                        uint32_t interval,
                        fp_callback fp_register,
                        void* arg);

uint8_t ticker_once_ms(ticker_function_handle_t *fp_ticker,
                        uint32_t interval,
                        fp_callback fp_register,
                        void* arg);

uint8_t allocate_ticker_once_ms(uint32_t interval,
                        fp_callback fp_register,
                        void* arg);

void ticker_detach(ticker_function_handle_t *fp_ticker);

void ticker_schedule_handler(void);

#ifdef __cplusplus
}
#endif

#endif	/* _TICKER_H_ */
