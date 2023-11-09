#ifndef __IO_INPUT_H
#define __IO_INPUT_H

#include <stdint.h>

#ifndef IO_LOW
#define IO_LOW		  0
#endif

#ifndef IO_HIGH
#define IO_HIGH	    1
#endif

#ifndef IO_RESET
#define IO_RESET	  2
#endif

#ifndef IO_RISING
#define IO_RISING	  3
#endif

#ifndef IO_FALLING
#define IO_FALLING	4
#endif

#define IO_INPUT_DEFAULT {0,        \
                          0,        \
                          0,        \
                          0,        \
                          0,        \
                          0,        \
                          0,        \
                          0,        \
                          IO_RESET, \
                          IO_RESET  \
                          }

struct io_input_cxt;

typedef struct io_input_cxt
{
  void *handle; /* Customizable optional pointer */
  void (*event_cb)(struct io_input_cxt*, uint8_t evt);
	uint32_t high_time;
	uint32_t low_time;
	uint16_t high_cnt;
	uint16_t low_cnt;
	uint16_t high_sample_cnt;
	uint16_t low_sample_cnt;
	uint8_t  state;
	uint8_t  edge;
}io_input_cxt_t;

void io_input_init(
    io_input_cxt_t *io_input,
    uint16_t high_sample_cnt,
    uint16_t low_sample_cnt);
void io_input_process(io_input_cxt_t *io_input, uint8_t);
uint32_t io_input_get_time(io_input_cxt_t *io_input, uint8_t);
uint8_t io_input_level_get(io_input_cxt_t *io_input);
uint8_t io_input_edge_get(io_input_cxt_t *io_input, uint8_t);
void io_input_sample_high_cnt_set(io_input_cxt_t *io_input, uint16_t);
void io_input_sample_low_cnt_set(io_input_cxt_t *io_input, uint16_t);
void io_input_set_state_init(io_input_cxt_t *io_input, uint8_t);

#endif
