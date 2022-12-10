#include <arduino.h>
#include "THIoT_PFAtCmdManage.h"
#include "THIoT_PFcUtil.h"
#include "THIoT_PFAtDevice.h"

#if (defined AT_CMD_HANDLER_DEBUG) && (AT_CMD_HANDLER_DEBUG == 1)
#include "THIoT_PFSerialTrace.h"
#define AT_CMD_HANDLER_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[AT_CMD_HANDLER]", __VA_ARGS__)
#define AT_CMD_HANDLER_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#else
#define AT_CMD_HANDLER_TAG_CONSOLE(...)
#define AT_CMD_HANDLER_CONSOLE(...)
#endif

/* Private typedef -----------------------------------------------------------*/
typedef enum {
  AT_CMD_TYPE_TEST = 0,
  AT_CMD_TYPE_QUERY,
  AT_CMD_TYPE_SETUP,
  AT_CMD_TYPE_EXE
} at_cmd_type_t;

/* Private define ------------------------------------------------------------*/
#define AT_DEVICE_RESP(W, f_, ...)    vspfunc(W, (f_), ##__VA_ARGS__)

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

void at_control_init(at_cmd_cxt_t *cxt, at_buffer_t *p_buffer, at_funcation_t *table, uint8_t size)
{
    cxt->cmd_table = table;
    cxt->cmd_num = size;
    at_device_init(cxt, p_buffer);
}

void at_test_attach_cb(at_cmd_cxt_t *cxt, uint8_t cmd, at_test_callback cb, void* arg)
{
  cxt->cmd_table[cmd].test_cmd = cb;
  cxt->cmd_table[cmd].arg = arg;
}

void at_test_detach_cb(at_cmd_cxt_t *cxt, uint8_t cmd)
{
  cxt->cmd_table[cmd].test_cmd = NULL;
}

void at_query_attach_cb(at_cmd_cxt_t *cxt, uint8_t cmd, at_query_callback cb, void* arg)
{
  cxt->cmd_table[cmd].query_cmd = cb;
  cxt->cmd_table[cmd].arg = arg;
}

void at_query_detach_cb(at_cmd_cxt_t *cxt, uint8_t cmd)
{
  cxt->cmd_table[cmd].query_cmd = NULL;
}

void at_exe_attach_cb(at_cmd_cxt_t *cxt, uint8_t cmd, at_exe_callback cb, void* arg)
{
  cxt->cmd_table[cmd].exe_cmd = cb;
  cxt->cmd_table[cmd].arg = arg;
}

void at_exe_detach_cb(at_cmd_cxt_t *cxt, uint8_t cmd)
{
  cxt->cmd_table[cmd].exe_cmd = NULL;
}

void at_setup_attach_cb(at_cmd_cxt_t *cxt, uint8_t cmd, at_setup_callback cb, void* arg)
{
  cxt->cmd_table[cmd].setup_cmd = cb;
  cxt->cmd_table[cmd].arg = arg;
}

void at_setup_detach_cb(at_cmd_cxt_t *cxt, uint8_t cmd)
{
  cxt->cmd_table[cmd].setup_cmd = NULL;
}
