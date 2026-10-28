#ifndef __TICKER_H
#define __TICKER_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <Arduino.h>
#include <functional>
#include "THIoT_APPConfig.h"

#define TICKER_FUNCTION_HANDLE_MAX  20	/* maximum number ticker function handler */
#define TICKER_FOREVER              0
#define TICKER_ONCE                 1

#if (MUTEX_PLATFORM_TICKER == 1)
#define TICKER_START_INIT(x)    iot_ticker((x), true)
#define TICKER_STOP_INIT        iot_ticker()
#else
#define TICKER_START_INIT(x)  {\
                              .start = millis(),\
                              .interval = (x),\
                              .status = TICKER_RUNNING\
                           }

#define TICKER_STOP_INIT   {\
                              .start = 0,\
                              .interval = 0,\
                              .status = TICKER_STOP\
                           }
#endif
typedef enum : uint8_t {
	TICKER_STOP = 0,
	TICKER_RUNNING
} ticker_status_t;

typedef struct iot_ticker {
	uint32_t start;
	uint32_t interval;
	ticker_status_t status;
#if (MUTEX_PLATFORM_TICKER == 1)
  SemaphoreHandle_t xSemaphore;
  iot_ticker(uint32_t timeout = 0, bool run = false);
  ~iot_ticker();
  iot_ticker& operator=(uint32_t timeout);
#endif
} ticker_t;

// typedef void (*fp_callback)(void*);
using fp_callback = std::function<void(void*)>;

typedef enum : uint8_t {
  TICKER_FUNCTION_FREE = 0,
  TICKER_FUNCTION_INSERTED,
  TICKER_FUNCTION_REJECT
} ticker_function_state_t;

typedef struct ticker_function_handle {
  ticker_t ticker;
  // fp_callback cb;
  std::function<void(void*)> cb;
  void *arg;
  uint16_t repeat;
  uint8_t heap_flag;
  uint8_t state;
} ticker_function_handle_t;

boolean ticker_expired(ticker_t *ticker);
void ticker_begin(ticker_t *ticker, uint32_t interval);
void ticker_stop(ticker_t *ticker);
ticker_status_t ticker_status(ticker_t *ticker);
uint32_t ticker_remain(ticker_t *ticker);
void ticker_refresh(ticker_t *ticker);
void ticker_restart(ticker_t *ticker);
void ticker_update(ticker_t *ticker, uint32_t interval);
void ticker_trigger(ticker_t *ticker, uint32_t timeout);

uint8_t ticker_attach(ticker_function_handle_t *fp_ticker,
                        uint32_t interval,
                        fp_callback fp_register,
                        void* arg);
                        
void ticker_attach_trigger(ticker_function_handle_t *fp_ticker,
                        uint32_t timeout);
void ticker_attach_ms_trigger(ticker_function_handle_t *fp_ticker,
                        uint32_t timeout);

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

ticker_function_handle_t *allocate_ticker_once_ms(uint32_t interval,
                                                  fp_callback fp_register,
                                                  void *arg);

void ticker_detach(ticker_function_handle_t *fp_ticker);

/**
 * @brief Main scheduler for ticker callbacks. Must be called from loopTask only.
 * 
 * INVARIANTS (P6):
 * - Single-threaded: exactly one task calls this function (enforced by assertion).
 * - Non-reentrant: safe to assume no unlink races during iteration.
 * 
 * P7: Removed IRAM_ATTR — callback may malloc/delete, not ISR-safe.
 */
void ticker_schedule_handler(void);

#if (MUTEX_PLATFORM_TICKER == 1)
/**
 * @brief Initialize the registry mutex. Must be called before any ticker_attach* call.
 * 
 * P14: Call this BEFORE spawning any task that uses tickers, to avoid silent races.
 */
void ticker_schedule_init(void);
#endif

#endif	// __TICKER_H
