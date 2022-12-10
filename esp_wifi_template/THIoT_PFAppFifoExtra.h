#ifndef APP_FIFO_EXTRA_H__
#define APP_FIFO_EXTRA_H__

/** Common include */
#include <stdint.h>
#include <stdbool.h>
#include "THIoT_PFAppFifo.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t app_fifo_length(app_fifo_t * const fifo);

uint32_t app_fifo_available(app_fifo_t * const fifo);

uint32_t app_fifo_buff_peek(app_fifo_t * p_fifo, uint8_t * p_byte_array, uint32_t * p_size);

#ifdef __cplusplus
}
#endif

#endif
