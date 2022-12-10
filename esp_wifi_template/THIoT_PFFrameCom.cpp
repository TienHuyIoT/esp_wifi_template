#include <arduino.h>
#include "THIoT_PFFrameCom.h"
#include "THIoT_PFTicker.h"

#if (defined FRAME_COM_DEBUG_ENABLE) && (FRAME_COM_DEBUG_ENABLE == 1)
#include "THIoT_PFSerialTrace.h"
#define FC_DBG_TAG_PRINTF(...) SERIAL_TAG_LOGI("[FRAME_COM]", __VA_ARGS__)
#define FC_DBG_PRINTF(...) SERIAL_LOGI(__VA_ARGS__)
#else
#define FC_DBG_PRINTF(f_, ...)
#define FC_DBG_TAG_PRINTF(f_, ...)
#endif

/* Private define ------------------------------------------------------------*/
#define FRAME_COM_CAPTURE_TIMEOUT 50 /* ms */

/**@brief String literals for the frame parse. */
const char* const lit_frameid[6] PROGMEM = {
	"FRAME_OK",
	"FRAME_ERR",
	"FRAME_SIZE_MIN_ERR",
	"FRAME_SOF_EOF_ERR",
	"FRAME_LENGTH_PACK_ERR",
	"FRAME_CRC_ERR"
};

/* Private function prototypes -----------------------------------------------*/
static uint8_t fc_capture_process(frame_com_cxt_t *fc, uint8_t c);

/* make an event callback function */
static void fc_parse_event(frame_com_cxt_t *fc, uint8_t *buff, frame_size length)
{
	frame_data_t Rx_Frame_t;
	frame_parse_result_t result;

	result = frame_data_parse(&Rx_Frame_t, buff, length);
	FC_DBG_TAG_PRINTF("result %s\r\n", FPSTR(lit_frameid[result]));

	/* assert pointer */
	if (fc->event_cb)
	{
		// run event callback function (frame_com_cxt_t*, result, cmd, data, length)
		fc->event_cb(fc, result, Rx_Frame_t.cmd, Rx_Frame_t.p_data, DATA_FIELD_SIZE(Rx_Frame_t.length));
	}
}

void frame_com_begin(
	frame_com_cxt_t *fc,
	frameCom_event_cb event_cb,
	fc_buffer_t *p_buffer)
{
	/* Init object handle communicate frame */
	fc->rx_buff = p_buffer->rx.buf;
	fc->rx_length = p_buffer->rx.size;
	fc->tx_buff = p_buffer->tx.buf;
	fc->tx_length = p_buffer->tx.size;
	fc->step = FRAME_COM_WAIT_START;
	ticker_stop(&fc->timeout);
	fc->event_cb = event_cb;
}

void frame_com_cmd_table_handle(frame_com_cxt_t *fc, frameCom_handle_t *table, uint8_t size)
{
	fc->cmd_table.index = table;
	fc->cmd_table.size = size;
}

void frame_com_process(frame_com_cxt_t *fc)
{
	uint8_t buf[1]; /* Purpose the address is 4-byte aligned here */
	frame_size get_cnt;

	if (fc->input_cb)
	{
		get_cnt = FRAME_SIZE_MAX / 2; /* counter limit once process get data */
		while (fc->input_cb(buf, 1))
		{
			uint8_t c = buf[0];

			if (FRAME_COM_FINISH == fc_capture_process(fc, c))
			{
				break;
			}

			// Avoid while loop forever if uart_rx so much
			--get_cnt;
			if (0 == get_cnt)
			{
				break;
			}
		}
	}
}

/* Brief: API make and send a frame
 * Callback should be an asynchronous handle to high performance
 * */
uint8_t frame_com_transmit(frame_com_cxt_t *fc, uint8_t cmd, uint8_t *data, uint16_t length)
{
	frame_data_t frame_create;
	frame_size tx_length;
	uint8_t result;

	/* create a frame */
	frame_data_create(&frame_create, cmd, data, length);
	tx_length = fc->tx_length;
	/* fill fields of frame into buff */
	result = frame_data_fill_buff(&frame_create, fc->tx_buff, &tx_length);
	if (FRAME_OK == result)
	{
		/* assert pointer */
		if (fc->output_cb)
		{
			fc->output_cb(fc->tx_buff, tx_length);
		}
	}
	return result;
}

/* Brief: capture frame and parse element
 * When the capture a frame is success, will executing process parse data
 * There are event callback has call to provide command and data
 * */
static uint8_t fc_capture_process(frame_com_cxt_t *fc, uint8_t c)
{
	FC_DBG_TAG_PRINTF("%02X", c);

	// check timeout to restart capture a new frame
	if (ticker_expired(&fc->timeout))
	{
		/* reset state machine equal FRAME_COM_WAIT_START */
		fc->step = FRAME_COM_WAIT_START;
		FC_DBG_TAG_PRINTF("Timeout command\r\n");
	}

	switch (fc->step)
	{
	case FRAME_COM_FINISH:
		fc->step = FRAME_COM_WAIT_START;
	case FRAME_COM_WAIT_START:
		if (c == FRAME_START_BYTE)
		{
			fc->step = FRAME_COM_GET_LENGTH;
			fc->index = FRAME_START_INDEX;
			fc->rx_buff[fc->index++] = c;

			FC_DBG_TAG_PRINTF("START");
			// active timeout start get a frame data
			ticker_begin(&fc->timeout, FRAME_COM_CAPTURE_TIMEOUT);
		}
		break;

	case FRAME_COM_GET_LENGTH:
		if (LENGTH_FIELD_SIZE_MIN > c || LENGTH_FIELD_SIZE(fc->rx_length) < c)
		{
			FC_DBG_TAG_PRINTF("length error (%u)\r\n", c);
			fc->step = FRAME_COM_WAIT_START;
			break;
		}
		fc->step = FRAME_COM_WAIT_STOP;
		fc->rx_buff[fc->index++] = c;
		fc->remain_byte_cnt = c + STOP_BYTE_NUM;
		FC_DBG_TAG_PRINTF("length byte 0x%02X\r\n", c);
		break;

	case FRAME_COM_WAIT_STOP:
		fc->rx_buff[fc->index++] = c;

		fc->remain_byte_cnt--;
		if (0 == fc->remain_byte_cnt)
		{
			FC_DBG_TAG_PRINTF("STOP");
			fc->step = FRAME_COM_FINISH;
			/* parse frame buffer */
			fc_parse_event(fc, fc->rx_buff, FRAME_SIZE(fc->rx_buff[FRAME_LENGTH_INDEX]));
			/* Stop ticker fc->timeout */
			ticker_stop(&fc->timeout);
		}
		break;

	default:
		fc->step = FRAME_COM_WAIT_START;
		break;
	}

	return fc->step;
}
