#include <stdbool.h>
#include <stdint.h>
#include <Arduino.h>
#include "THIoT_PFIoInput.h"

void io_input_init(
    io_input_cxt_t *io_input,
    uint16_t high_sample_cnt,
    uint16_t low_sample_cnt)
{
  io_input->state = IO_RESET;
  io_input->edge = IO_RESET;
  io_input->high_sample_cnt = high_sample_cnt;
  io_input->low_sample_cnt  = low_sample_cnt;
}

void io_input_process(io_input_cxt_t *io_input, uint8_t level)
{
  if (IO_LOW == level)
  {
    io_input->low_cnt++;
    if (io_input->low_cnt == io_input->low_sample_cnt)
    {
      if (IO_HIGH == io_input->state)
      {
        io_input->edge = IO_FALLING;
        /* event callback process */
        if (io_input->event_cb)
        {
          io_input->event_cb(io_input, IO_FALLING);
        }
      }
      io_input->state = IO_LOW;
      io_input->low_time = millis();
    }
    if (io_input->low_cnt >= io_input->low_sample_cnt)
    {
      io_input->high_cnt = 0;
      io_input->low_cnt = io_input->low_sample_cnt;
    }
  } else
  {
    io_input->high_cnt++;
    if (io_input->high_cnt == io_input->high_sample_cnt)
    {
      if (IO_LOW == io_input->state)
      {
        io_input->edge = IO_RISING;
        /* event callback process */
        if (io_input->event_cb)
        {
          io_input->event_cb(io_input, IO_RISING);
        }
      }
      io_input->state = IO_HIGH;
      io_input->high_time = millis();
    }
    if (io_input->high_cnt >= io_input->high_sample_cnt)
    {
      io_input->low_cnt = 0;
      io_input->high_cnt = io_input->high_sample_cnt;
    }
  }

  /* event callback process */
  if (io_input->event_cb)
  {
    io_input->event_cb(io_input, 255);
  }
}

uint32_t io_input_get_time(io_input_cxt_t *io_input, uint8_t level)
{
  uint32_t time = 0;
  if (level == io_input->state)
  {
    if (IO_HIGH == io_input->state)
    {
      time = millis() - io_input->high_time;
    } else
    {
      time = millis() - io_input->low_time;
    }
  }

  return time;
}

uint8_t io_input_edge_get(io_input_cxt_t *io_input, uint8_t edge)
{
  if (io_input->edge == edge)
  {
    io_input->edge = IO_RESET;
    return true;
  }
  return false;
}

uint8_t io_input_level_get(io_input_cxt_t *io_input)
{
  return io_input->state;
}

void io_input_sample_high_cnt_set(io_input_cxt_t *io_input, uint16_t cnt)
{
  io_input->high_sample_cnt = cnt;
}

void io_input_sample_low_cnt_set(io_input_cxt_t *io_input, uint16_t cnt)
{
  io_input->low_sample_cnt = cnt;
}

void io_input_set_state_init(io_input_cxt_t *io_input, uint8_t St)
{
  io_input->state = St;
}
