#ifndef __AT_CMD_HANDLER_H
#define __AT_CMD_HANDLER_H

#include <stdint.h>
#include "THIoT_PFAtDevice.h"

#define AT_CMD_HANDLER_DEBUG 0

void at_control_init(at_cmd_cxt_t *cxt, at_buffer_t *p_buffer, at_function_t *table, uint8_t size);
void at_test_attach_cb(at_cmd_cxt_t *cxt, uint8_t cmd, at_test_callback cb, void* arg = nullptr);
void at_test_detach_cb(at_cmd_cxt_t *cxt, uint8_t cmd);
void at_query_attach_cb(at_cmd_cxt_t *cxt, uint8_t cmd, at_query_callback cb, void* arg = nullptr);
void at_query_detach_cb(at_cmd_cxt_t *cxt, uint8_t cmd);
void at_exe_attach_cb(at_cmd_cxt_t *cxt, uint8_t cmd, at_exe_callback cb, void* arg = nullptr);
void at_exe_detach_cb(at_cmd_cxt_t *cxt, uint8_t cmd);
void at_setup_attach_cb(at_cmd_cxt_t *cxt, uint8_t cmd, at_setup_callback cb, void* arg = nullptr);
void at_setup_detach_cb(at_cmd_cxt_t *cxt, uint8_t cmd);

#endif // __AT_CMD_HANDLER_H