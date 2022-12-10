#ifndef __FRAME_CONTROL_H
#define __FRAME_CONTROL_H

#include <Arduino.h>
#include <stdlib.h>
#include "THIoT_PFFrameCom.h"

#define FRAME_CONTROL_PORT Serial
#define FRAME_CONTROL_BAUD 115200U
#define FRAME_CONTROL_DEBUG 0

#define FRAME_CONTROL_TX_BUFF_SIZE 64
#define FRAME_CONTROL_RX_BUFF_SIZE 32

typedef enum
{
  FRAME_UART_INTERFACE = 0,
  FRAME_RS232_INTERFACE
} frame_control_instance_t;

/* ACK data frame element*/
#define ACK_DF_LENGTH               1  /* number of data field */
#define ACK_DF_ACK_INDEX            0  /* length is 1 byte */
#define FRAME_IF_BYTE_ACK           0
#define FRAME_IF_BYTE_NACK          1

void frame_control_init(frame_com_cxt_t *fc, fc_buffer_t *p_buffer, frameCom_handle_t *table, uint8_t size);
void frame_control_attach_once_cb(frame_com_cxt_t* fc, uint8_t cmd, frameCom_handler_once cb, void* arg);
void frame_control_detach_once_cb(frame_com_cxt_t* fc, uint8_t cmd);
void frame_control_attach_cb(frame_com_cxt_t* fc, uint8_t cmd, frameCom_handler cb, void* arg);
void frame_control_detach_cb(frame_com_cxt_t* fc, uint8_t cmd);
void frame_control_transmit(frame_com_cxt_t* fc, uint8_t cmd, uint8_t* data, uint16_t length, frameCom_handler_once handler, void* arg);
void frame_receive_cmd_callback(frame_com_cxt_t* frame_instance, uint8_t result, uint8_t cmd, uint8_t* data, uint16_t length);
void frame_control_ack_response(frame_com_cxt_t *fc, uint8_t cmd);
void frame_control_nack_response(frame_com_cxt_t *fc, uint8_t cmd);

#endif // __FRAME_CONTROL_H