#include <arduino.h>
#include "THIoT_PFFrame.h"
#include "THIoT_PFFrameCom.h"
#include "THIoT_PFFrameCmdManage.h"

#if (defined FRAME_CONTROL_DEBUG) && (FRAME_CONTROL_DEBUG == 1)
#include "THIoT_SerialTrace.h"
#define FRAME_CONTROL_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[FRAME_CONTROL]", __VA_ARGS__)
#define FRAME_CONTROL_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#else
#define FRAME_CONTROL_TAG_CONSOLE(...)
#define FRAME_CONTROL_CONSOLE(...)
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

#if (defined FRAME_CONTROL_DEBUG) && (FRAME_CONTROL_DEBUG == 1)
const char *const lit_frame_result_id[] PROGMEM = {
    "FRAME_OK",
    "FRAME_ERR",
    "FRAME_SIZE_MIN_ERR",
    "FRAME_SOF_EOF_ERR",
    "FRAME_LENGTH_PACK_ERR",
    "FRAME_CRC_ERR"};
#endif

/* Private function prototypes -----------------------------------------------*/

void frame_control_init(frame_com_cxt_t *fc, fc_buffer_t *p_buffer, frameCom_handle_t *table, uint8_t size)
{
    fc->cmd_table.index = table;
    fc->cmd_table.size = size;
    frame_com_begin(fc, frame_receive_cmd_callback, p_buffer);
}

void frame_control_attach_once_cb(frame_com_cxt_t* fc, uint8_t cmd, frameCom_handler_once cb, void* arg)
{
    for (uint8_t i = 0; i < fc->cmd_table.size; ++i)
    {
        /* Find command handle */
        if (fc->cmd_table.index[i].command == cmd)
        {
            fc->cmd_table.index[i].once_process_cb = cb;
            fc->cmd_table.index[i].once_arg = arg;
            break;
        }
    }
}

void frame_control_detach_once_cb(frame_com_cxt_t* fc, uint8_t cmd)
{
    for (uint8_t i = 0; i < fc->cmd_table.size; ++i)
    {
        /* Find command handle */
        if (fc->cmd_table.index[i].command == cmd)
        {
            fc->cmd_table.index[i].once_process_cb = nullptr;
            fc->cmd_table.index[i].once_arg = NULL;
            break;
        }
    }
}

void frame_control_attach_cb(frame_com_cxt_t* fc, uint8_t cmd, frameCom_handler cb, void* arg)
{
    for (uint8_t i = 0; i < fc->cmd_table.size; ++i)
    {
        /* Find command handle */
        if (fc->cmd_table.index[i].command == cmd)
        {
            fc->cmd_table.index[i].process_cb = cb;
            fc->cmd_table.index[i].arg = arg;
            break;
        }
    }
}

void frame_control_detach_cb(frame_com_cxt_t* fc, uint8_t cmd)
{
    for (uint8_t i = 0; i < fc->cmd_table.size; ++i)
    {
        /* Find command handle */
        if (fc->cmd_table.index[i].command == cmd)
        {
            fc->cmd_table.index[i].process_cb = nullptr;
            fc->cmd_table.index[i].arg = NULL;
            break;
        }
    }
}

/* Brief: API send frame command to EVSE
 * [cmd]: command refer from rfid_command_t
 * [data]: data buffer shall fill to data's field of frame command
 * [length]: the length of data buffer
 * */
void frame_control_transmit(frame_com_cxt_t *fc, uint8_t cmd, uint8_t *data,
                       uint16_t length, frameCom_handler_once handler, void* arg)
{
    FRAME_CONTROL_CONSOLE("\r\n[%u][ Send frame ]", fc->instance);
    FRAME_CONTROL_CONSOLE("\r\n- CMD: %u", cmd);
    FRAME_CONTROL_CONSOLE("\r\n- Data length: %u", length);
    FRAME_CONTROL_CONSOLE("\r\n- Data: ");
    if (0 != length)
    {
        for (uint16_t i = 0; i < length; ++i)
        {
            FRAME_CONTROL_CONSOLE("{%02X} ", data[i]);
        }
    }
    else
    {
        FRAME_CONTROL_CONSOLE("NONE");
    }
    FRAME_CONTROL_CONSOLE("\r\n");
    
    if (handler)
    {
        frame_control_attach_once_cb(fc, cmd, handler, arg);
    }

    frame_com_transmit(fc, cmd, data, length);
}

/* Brief: the function event callback parsed a frame message */
void frame_receive_cmd_callback(frame_com_cxt_t *fc, uint8_t result, uint8_t cmd,
                             uint8_t *data, uint16_t length)
{
    FRAME_CONTROL_CONSOLE("\r\n[%u] Result Receive command: %s", fc->instance, lit_frame_result_id[result]);

    if ((uint8_t)FRAME_OK == result)
    {
        frameCom_handle_t *p_cmd = fc->cmd_table.index;

        FRAME_CONTROL_CONSOLE("\r\n[%u][ Get new frame ]", fc->instance);
        FRAME_CONTROL_CONSOLE("\r\n- CMD: %u", cmd);
        FRAME_CONTROL_CONSOLE("\r\n- Data length: %u", length);
        FRAME_CONTROL_CONSOLE("\r\n- Data: ");
        for (uint16_t i = 0; i < length; ++i)
        {
            FRAME_CONTROL_CONSOLE("{%02X} ", data[i]);
        }
        FRAME_CONTROL_CONSOLE("\r\n");

        for (uint8_t i = 0; i < fc->cmd_table.size; ++i)
        {
            /* Find command handle */
            if (p_cmd[i].command == cmd)
            {
                /* Assert callback function */
                if (p_cmd[i].process_cb)
                {
                    p_cmd[i].process_cb(fc, i, data, length);
                }

                if (p_cmd[i].once_process_cb)
                {
                    frameCom_handler_once callback;
                    callback = p_cmd[i].once_process_cb;
                    p_cmd[i].once_process_cb = nullptr;
                    // the callback function can be register again by itself
                    // So it must be call after once_process_cb = nullptr
                    callback(p_cmd[i].once_arg, data, length);
                }
                break;
            } // if (p_cmd[i].command == cmd)
        } // For()
    } // if((uint8_t)FRAME_OK == result)
}

void frame_control_ack_response(frame_com_cxt_t *fc, uint8_t cmd)
{
    uint8_t data_buf[ACK_DF_LENGTH];
    data_buf[ACK_DF_ACK_INDEX] = FRAME_IF_BYTE_ACK;
    frame_control_transmit(fc, (uint8_t)cmd, data_buf, ACK_DF_LENGTH, nullptr, NULL);
    FRAME_CONTROL_CONSOLE("\r\n[%u][%s] ACK response", fc->instance, lit_command_id[cmd]);
    FRAME_CONTROL_CONSOLE("\r\n");
}

void frame_control_nack_response(frame_com_cxt_t *fc, uint8_t cmd)
{
    uint8_t data_buf[ACK_DF_LENGTH];
    data_buf[ACK_DF_ACK_INDEX] = FRAME_IF_BYTE_NACK;
    frame_control_transmit(fc, (uint8_t)cmd, data_buf, ACK_DF_LENGTH, nullptr, NULL);
    FRAME_CONTROL_CONSOLE("\r\n[%u][%s] NACK response", fc->instance, lit_command_id[cmd]);
    FRAME_CONTROL_CONSOLE("\r\n");
}
