#ifndef APP_FIFO_EXTRA_H__
#define APP_FIFO_EXTRA_H__

/** Common include */
#include <stdint.h>
#include <stdbool.h>
#include "THIoT_PFAppFifo.h"

inline uint32_t app_fifo_length(app_fifo_t * const fifo)
{
  uint32_t tmp = fifo->read_pos;
  return fifo->write_pos - tmp;
}

uint32_t app_fifo_available_count(app_fifo_t * const fifo);

uint32_t app_fifo_buff_peek(app_fifo_t * p_fifo, uint8_t * p_byte_array, uint32_t * p_size);

#endif
