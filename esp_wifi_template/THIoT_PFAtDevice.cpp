#include <arduino.h>
#include <stdarg.h>
#include <string.h>
#include "THIoT_PFTicker.h"
#include "THIoT_PFcUtil.h"
#include "THIoT_PFAtDevice.h"

#if (defined AT_CMD_DEBUG) && (AT_CMD_DEBUG == 1)
#include "THIoT_PFSerialTrace.h"
#define AT_PRINT(...) SERIAL_LOGI(__VA_ARGS__)
#define AT_TAG_PRINT(...) SERIAL_TAG_LOGI("[AT]", __VA_ARGS__)
#else
#define AT_PRINT(f_, ...)
#define AT_TAG_PRINT(f_, ...)
#endif

#define AT_CMD_RESP(f_, ...)    vspfunc(at->output_cb, (f_), ##__VA_ARGS__)

#define AT_END_OF_CHAR        '\r'
#define CHAR_CAPTURE_LIMIT    30
#define AT_CAPTURE_TIMEOUT    50 /* ms */

const char at_head[]          = "06AT";
const uint8_t AT_HEAD_LENGTH  = 4;
const char at_cmd_ok[]        = "\r\nOK\r\n";
const uint8_t AT_OK_LENGTH    = 6;
const char at_cmd_error[]     = "\r\nERROR\r\n";
const uint8_t AT_ERROR_LENGTH = 9;

static int at_cmd_get_length(at_cmd_cxt_t *at);
static void at_cmd_parse(at_cmd_cxt_t* at);

void at_device_init(at_cmd_cxt_t *at, at_buffer_t *p_buffer)
{
  at->at_id = 0;
  at->buff_id = 0;
  at->at_capture = 0;
  at->buff = p_buffer->buf;
  at->size = p_buffer->size;
  ticker_stop(&at->timeout);
}

void at_device_handle(at_cmd_cxt_t *at)
{
  uint8_t buf[1]; /* Purpose the address is 4-byte aligned here */
  size_t cnt_limit;
  char c;

  /* Timeout to reset Process at capture */
  if (ticker_expired(&at->timeout))
  {
    at->at_capture = 0;
    at->at_id = 0;
  }

  cnt_limit = CHAR_CAPTURE_LIMIT;
  while (at->input_cb(buf, 1))
  {
    c = buf[0];

    if (!at->at_capture)
    {
      if (check_strncmp((const char*)at_head, buf[0], &at->at_id, AT_HEAD_LENGTH))
      {
        at->at_capture = 1;

        ticker_begin(&at->timeout, AT_CAPTURE_TIMEOUT);
        at->buff_id = 0;
        cnt_limit = CHAR_CAPTURE_LIMIT;
      }
    }
    else
    {
      at->buff[at->buff_id++] = c;
      if (at->size == at->buff_id)
      {
        at->at_capture = 0;
        /* Stop timeout */
        ticker_stop(&at->timeout);
      }

      if (AT_END_OF_CHAR == c)
      {
        at->buff[at->buff_id] = '\0'; /* Add null terminal character */
        at->at_capture = 0;
        at_cmd_parse(at);

        /* Stop timeout */
        ticker_stop(&at->timeout);
        break;
      }
    }

    // Avoid while loop forever
    --cnt_limit;
    if(0 == cnt_limit) {
      ticker_stop(&at->timeout);
      break;
    }

  }
}

static int at_cmd_get_length(at_cmd_cxt_t *at)
{
  char* s = at->buff;
  size_t size = at->size;
  int index = -1;

  while(*s && size)
  {
    char c = *s;
    ++s;

    if((c == '\r') || (c == '=') || (c == '?')
        || ((c >= '0') && (c <= '9')))
    {
      index = at->size - size;
      break;
    }
    else
    {
      --size;
    }
  }
  AT_TAG_PRINT("CMD Length %d", index);
  return index;
}

static void at_cmd_parse(at_cmd_cxt_t* at)
{
  char* s;
  at_funcation_t* p_cmd_handle;
  int i;
  int cmd_length;
  int cmd_index = -1;

  cmd_length = at_cmd_get_length(at);
  if(cmd_length != -1)
  {
    for(i = 0; i < at->cmd_num; ++i)
    {
      if(cmd_length == at->cmd_table[i].cmd_len)
      {
        if(memcmp(at->buff, at->cmd_table[i].cmd_name, cmd_length) == 0)
        {
          cmd_index = i;
          break;
        }
      }
    }
  }

  AT_TAG_PRINT("CMD index %d", cmd_index);

  if(cmd_index != -1)
  {
    p_cmd_handle = (at_funcation_t*)&at->cmd_table[cmd_index];
    AT_TAG_PRINT("Process Command AT%s", p_cmd_handle->cmd_name);
    /* Add write pointer to output callback */
    p_cmd_handle->write = at->output_cb;
    s = at->buff + cmd_length;

    if(s[0] == '\r')
    {
      if(p_cmd_handle->exe_cmd)
      {
        p_cmd_handle->exe_cmd(p_cmd_handle);
      }
      else
      {   
        at->output_cb((const uint8_t*)at_cmd_error, AT_ERROR_LENGTH);
      }
    }
    else if(s[0] == '?' && (s[1] == '\r'))
    {
      if(p_cmd_handle->query_cmd)
      {
        p_cmd_handle->query_cmd(p_cmd_handle);
      }
      else
      {
        at->output_cb((const uint8_t*)at_cmd_error, AT_ERROR_LENGTH);
      }
    }
    else if((s[0] == '=') && (s[1] == '?') && (s[2] == '\r'))
    {
      if(p_cmd_handle->test_cmd)
      {
        p_cmd_handle->test_cmd(p_cmd_handle);
      }
      else
      {
        at->output_cb((const uint8_t*)at_cmd_error, AT_ERROR_LENGTH);
      }
    }
    else if(((s[0] >= '0') && (s[0] <= '9')) || (s[0] == '='))
    {
      if(p_cmd_handle->setup_cmd)
      {
        char* cr = strrchr(s, '\r');
        if (cr)
        {
          *cr = '\0'; /* remove CR character */
        }
        ++s;
        p_cmd_handle->setup_cmd(p_cmd_handle, s);
      }
      else
      {
        at->output_cb((const uint8_t*)at_cmd_error, AT_ERROR_LENGTH);
      }
    }
    else
    {
      at->output_cb((const uint8_t*)at_cmd_error, AT_ERROR_LENGTH);
    }
  }
  else
  {
    at->output_cb((const uint8_t*)at_cmd_error, AT_ERROR_LENGTH);
  }

  AT_PRINT("\r\n");
}
