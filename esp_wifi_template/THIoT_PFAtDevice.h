#ifndef __AT_CMD_H
#define __AT_CMD_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "THIoT_PFTicker.h"

#define AT_CMD_DEBUG    0

typedef size_t (*at_rx_cb)(uint8_t*, size_t);
typedef size_t (*at_tx_cb)(const uint8_t*, size_t);

typedef struct
{
  char *buf;
  size_t size;
} at_buffer_t;

/**
 * @brief at_cmd_struct
 *  used for define at command
 *
 */
struct at_funcation;
typedef void (*at_test_callback)(struct at_funcation*);
typedef void (*at_query_callback)(struct at_funcation*);
typedef void (*at_exe_callback)(struct at_funcation*);
typedef void (*at_setup_callback)(struct at_funcation*, char *);
typedef struct at_funcation {
    const char *cmd_name;                            /*!< at command name */
    int8_t cmd_len;                            /*!< at command length */
    at_test_callback test_cmd;   /*!< Test Command function pointer AT+<CMD>=?\r */
    at_query_callback query_cmd; /*!< Query Command function pointer AT+<CMD>?\r */
    at_setup_callback setup_cmd; /*!< Setup Command function pointer AT+<CMD>=<data>\r */
    at_exe_callback exe_cmd;     /*!< Execute Command function pointer AT+<CMD>\r */
    at_tx_cb write;                            /*!< Execute write to framework output */
    void* arg;
} at_funcation_t;

typedef struct
{
  at_rx_cb input_cb;
  at_tx_cb output_cb;
  at_funcation_t* cmd_table;
  size_t cmd_num;
  ticker_t timeout;
  size_t size;        /* Size of buffer */
  char *buff;         /* Pointer to buffer process at command */
  uint8_t at_id;
  uint8_t at_capture;
  uint8_t buff_id;
} at_cmd_cxt_t;

/* [AT_BUF_SIZE + 1] reserve NULL terminal*/
#define AT_DEVICE_INIT(P_COMM_PARAMS, AT_BUF_SIZE) \
  do                                               \
  {                                                \
    at_buffer_t buffer;                            \
    static uint8_t buf[AT_BUF_SIZE + 1];           \
    buffer.buf = buf;                              \
    buffer.size = sizeof(buf);                     \
    at_device_init(P_COMM_PARAMS, &buffer);        \
  } while (0)

extern const char at_head[];
extern const char at_cmd_ok[];
extern const char at_cmd_error[];
extern const uint8_t AT_HEAD_LENGTH;
extern const uint8_t AT_OK_LENGTH;
extern const uint8_t AT_ERROR_LENGTH;

void at_device_init(at_cmd_cxt_t *at, at_buffer_t *p_buffer);

void at_device_handle(at_cmd_cxt_t *at);

#ifdef __cplusplus
}
#endif

#endif
