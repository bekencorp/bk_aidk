#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#include "bk_wss.h"
#include "bk_wss_private.h"
#include "bk_wss_config.h"
#if CONFIG_ARCH_CM33
#include <driver/aon_rtc.h>
#endif
#include "bk_genie_comm.h"
#include "app_event.h"
#include <modules/wifi.h>
#include "modules/wifi_types.h"


#define TAG "BEKEN_WSS"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)
void _rtc_websocket_audio_receive_data(rtc_session *rtc_session, uint8 *data, uint32_t len, rtc_user_audio_rx_data_handle_cb cb);
void _rtc_websocket_audio_receive_text(cJSON *root);

#define DEBUG_CRC
#define BEKEN_RTC_TXT_SIZE 500
static const uint8 crc8_table[256] =
{
	0x00, 0xF7, 0xB9, 0x4E, 0x25, 0xD2, 0x9C, 0x6B,
	0x4A, 0xBD, 0xF3, 0x04, 0x6F, 0x98, 0xD6, 0x21,
	0x94, 0x63, 0x2D, 0xDA, 0xB1, 0x46, 0x08, 0xFF,
	0xDE, 0x29, 0x67, 0x90, 0xFB, 0x0C, 0x42, 0xB5,
	0x7F, 0x88, 0xC6, 0x31, 0x5A, 0xAD, 0xE3, 0x14,
	0x35, 0xC2, 0x8C, 0x7B, 0x10, 0xE7, 0xA9, 0x5E,
	0xEB, 0x1C, 0x52, 0xA5, 0xCE, 0x39, 0x77, 0x80,
	0xA1, 0x56, 0x18, 0xEF, 0x84, 0x73, 0x3D, 0xCA,
	0xFE, 0x09, 0x47, 0xB0, 0xDB, 0x2C, 0x62, 0x95,
	0xB4, 0x43, 0x0D, 0xFA, 0x91, 0x66, 0x28, 0xDF,
	0x6A, 0x9D, 0xD3, 0x24, 0x4F, 0xB8, 0xF6, 0x01,
	0x20, 0xD7, 0x99, 0x6E, 0x05, 0xF2, 0xBC, 0x4B,
	0x81, 0x76, 0x38, 0xCF, 0xA4, 0x53, 0x1D, 0xEA,
	0xCB, 0x3C, 0x72, 0x85, 0xEE, 0x19, 0x57, 0xA0,
	0x15, 0xE2, 0xAC, 0x5B, 0x30, 0xC7, 0x89, 0x7E,
	0x5F, 0xA8, 0xE6, 0x11, 0x7A, 0x8D, 0xC3, 0x34,
	0xAB, 0x5C, 0x12, 0xE5, 0x8E, 0x79, 0x37, 0xC0,
	0xE1, 0x16, 0x58, 0xAF, 0xC4, 0x33, 0x7D, 0x8A,
	0x3F, 0xC8, 0x86, 0x71, 0x1A, 0xED, 0xA3, 0x54,
	0x75, 0x82, 0xCC, 0x3B, 0x50, 0xA7, 0xE9, 0x1E,
	0xD4, 0x23, 0x6D, 0x9A, 0xF1, 0x06, 0x48, 0xBF,
	0x9E, 0x69, 0x27, 0xD0, 0xBB, 0x4C, 0x02, 0xF5,
	0x40, 0xB7, 0xF9, 0x0E, 0x65, 0x92, 0xDC, 0x2B,
	0x0A, 0xFD, 0xB3, 0x44, 0x2F, 0xD8, 0x96, 0x61,
	0x55, 0xA2, 0xEC, 0x1B, 0x70, 0x87, 0xC9, 0x3E,
	0x1F, 0xE8, 0xA6, 0x51, 0x3A, 0xCD, 0x83, 0x74,
	0xC1, 0x36, 0x78, 0x8F, 0xE4, 0x13, 0x5D, 0xAA,
	0x8B, 0x7C, 0x32, 0xC5, 0xAE, 0x59, 0x17, 0xE0,
	0x2A, 0xDD, 0x93, 0x64, 0x0F, 0xF8, 0xB6, 0x41,
	0x60, 0x97, 0xD9, 0x2E, 0x45, 0xB2, 0xFC, 0x0B,
	0xBE, 0x49, 0x07, 0xF0, 0x9B, 0x6C, 0x22, 0xD5,
	0xF4, 0x03, 0x4D, 0xBA, 0xD1, 0x26, 0x68, 0x9F
};

#if CONFIG_PSRAM
#define WSS_AUDIO_BUFFER_SIZE (680*1024)
#else
#define WSS_AUDIO_BUFFER_SIZE (68*1024)
#endif

void rtc_client_hex_dump(uint8_t *data, uint32_t length)
{
	for (int i = 0; i < length; i++)
		BK_RAW_LOGI(NULL, "%02X ", *(u8 *)(data+i));
	BK_RAW_LOGI(NULL, "\r\n");
}

data_buffer_fixed_t *fixed_data_buffer_init(size_t buffer_size) {
#if CONFIG_PSRAM_AS_SYS_MEMORY
	data_buffer_fixed_t *ab = (data_buffer_fixed_t *)psram_malloc(sizeof(data_buffer_fixed_t));
#else
	data_buffer_fixed_t *ab = (data_buffer_fixed_t *)os_malloc(sizeof(data_buffer_fixed_t));
#endif
	if (ab == NULL)
	{
		LOGE("malloc ab fail\n");
		return NULL;
	}
	memset(ab, 0, sizeof(data_buffer_fixed_t));
#if CONFIG_PSRAM_AS_SYS_MEMORY
	ab->buffer = psram_malloc(buffer_size);
#else
	ab->buffer = os_malloc(buffer_size);
#endif
	if (ab->buffer == NULL)
	{
		LOGE("malloc data buffer fail\n");
		return NULL;
	}
	memset(ab->buffer, 0, buffer_size);
	ab->head = 0;
	ab->tail = 0;
	ab->size = buffer_size;

	return ab;
}

void fixed_data_buffer_deinit(data_buffer_fixed_t *ab) {
	if (ab == NULL)
	{
		LOGE("buffer deinit already\n");
		return;
	}
	memset(ab->buffer, 0, ab->size);
	if (ab->buffer)
	{
		os_free(ab->buffer);
	}
	ab->head = 0;
	ab->tail = 0;
	ab->size = 0;
	memset(ab, 0, sizeof(data_buffer_fixed_t));
	if(ab) {
		os_free(ab);
	}
}

bool fixed_data_buffer_write(data_buffer_fixed_t *ab, const uint8_t *data, size_t len) {

	if (len > ab->size) {
		return false;
	}

	size_t free_space = (ab->tail > ab->head) ? (ab->tail - ab->head) : (ab->size - ab->head + ab->tail);
	if (free_space < len) {
		return false;
	}

	if (ab->head + len <= ab->size) {
		memcpy(&ab->buffer[ab->head], data, len);
	} else {
		size_t first_part = ab->size - ab->head;
		memcpy(&ab->buffer[ab->head], data, first_part);
		memcpy(ab->buffer, data + first_part, len - first_part);
	}

	ab->head = (ab->head + len) % ab->size;

	return true;
}

bool fixed_data_buffer_read(data_buffer_fixed_t *ab, uint8_t *data, size_t len) {
	if (len > ab->size) {
		return false;
	}

	size_t available_data = (ab->head >= ab->tail) ? (ab->head - ab->tail) : (ab->size - ab->tail + ab->head);
	if (available_data < len) {
		return false;
	}

	if (ab->tail + len <= ab->size) {
		memcpy(data, &ab->buffer[ab->tail], len);
	} else {
		size_t first_part = ab->size - ab->tail;
		memcpy(data, &ab->buffer[ab->tail], first_part);
		memcpy(data + first_part, ab->buffer, len - first_part);
	}

	ab->tail = (ab->tail + len) % ab->size;
	return true;
}

void fixed_data_check(void *param)
{
	rtc_session *session = (rtc_session *) param;
	if(session == NULL) {
		BK_LOGE(TAG, "session null...\n");
		return;
	}
	uint8_t *packet = NULL;
	packet = os_zalloc(session->audio_info.dec_node_size + HEAD_SIZE_TOTAL);
	if (packet != NULL)
	{
		rtos_lock_mutex(&session->rtc_mutex);
		if (session->ab_buffer && fixed_data_buffer_read(session->ab_buffer, packet, (session->audio_info.dec_node_size + HEAD_SIZE_TOTAL)) && session) {
			_rtc_websocket_audio_receive_data(session, packet, (session->audio_info.dec_node_size + HEAD_SIZE_TOTAL), session->rtc_channel_t->cb);
			LOGD("data coming...\n");
		} else {
			LOGD("Buffer empty, waiting for data...\n");
		}
		rtos_unlock_mutex(&session->rtc_mutex);
	}
	os_free(packet);
}

data_buffer_t *data_buffer_init (size_t buffer_size, size_t length_buffer_size) {
#if CONFIG_PSRAM_AS_SYS_MEMORY
	data_buffer_t *rb = (data_buffer_t *)psram_malloc(sizeof(data_buffer_t));
#else
	data_buffer_t *rb = (data_buffer_t *)os_malloc(sizeof(data_buffer_t));
#endif
	if (rb == NULL)
	{
		LOGE("malloc rb fail\n");
		return NULL;
	}
	memset(rb, 0, sizeof(data_buffer_t));
#if CONFIG_PSRAM_AS_SYS_MEMORY
	rb->buffer = (uint8_t*) psram_malloc (buffer_size);
#else
	rb->buffer = (uint8_t*) os_malloc (buffer_size);
#endif
	if (rb->buffer == NULL)
	{
		LOGE("malloc buffer_size fail\n");
		os_free(rb);
		return NULL;
	}

	memset(rb->buffer, 0, buffer_size);
	rb->size = buffer_size;
	rb->read_index = 0;
	rb->write_index = 0;
#if CONFIG_PSRAM_AS_SYS_MEMORY
	rb->length_buffer = (size_t*) psram_malloc (length_buffer_size * sizeof (size_t));
#else
	rb->length_buffer = (size_t*) os_malloc (length_buffer_size * sizeof (size_t));
#endif
	if (rb->length_buffer == NULL)
	{
		LOGE("malloc length_buffer fail\n");
		os_free(rb);
		os_free(rb->buffer);
		return NULL;
	}
	memset((size_t*)rb->length_buffer, 0, length_buffer_size * sizeof (size_t));
	rb->length_read_index = 0;
	rb->length_write_index = 0;
	rb->buffer_count = length_buffer_size;
	LOGE("ringbuffer size:%d ringbuffer max count:%d\n", buffer_size, length_buffer_size);
	return rb;
}

void data_buffer_deinit(data_buffer_t *rb) {

	if (rb == NULL)
	{
		LOGE("buffer deinit already\n");
		return;
	}

	if (rb->buffer)
	{
		os_free(rb->buffer);
	}

	if (rb->length_buffer)
	{
		os_free (rb->length_buffer);
	}

	memset(rb, 0, sizeof(data_buffer_t));
	if (rb)
	{
		os_free(rb);
	}
}

void data_buffer_write (data_buffer_t* rb, const uint8_t *data, size_t data_len) {

	size_t remaining = rb->size - rb->write_index;
    if ((rb->length_write_index + 1) % rb->buffer_count == rb->length_read_index) {
		LOGE("%s, write buffer fail length_write_index:%d write_index:%d read_index:%d data_len:%d\r\n",
			__func__, rb->length_write_index, rb->write_index, rb->read_index, data_len);
        return;
    }

	if (data_len <= remaining) {
		memcpy (&rb->buffer [rb->write_index], data, data_len);
		rb->write_index += data_len;
	} else {
		size_t first_part = remaining;
		memcpy (&rb->buffer [rb->write_index], data, first_part);
		size_t second_part = data_len - first_part;
		memcpy (rb->buffer, &data [first_part], second_part);
		rb->write_index = second_part;
	}
	rb->length_buffer [rb->length_write_index] = data_len;
	rb->length_write_index = (rb->length_write_index + 1) % rb->buffer_count;
}

size_t data_buffer_read(data_buffer_t *rb, uint8_t *output) {

	if ((rb->length_read_index == rb->length_write_index) && (rb->read_index == rb->write_index)) {
		return 0;
	}
	size_t data_len = rb->length_buffer [rb->length_read_index];
	size_t available = (rb->write_index >= rb->read_index)? (rb->write_index - rb->read_index) : (rb->size - rb->read_index + rb->write_index);
	if (available < data_len) {
		return 0;
	}
	if (rb->write_index >= rb->read_index) {
		memcpy (output, &rb->buffer [rb->read_index], data_len);
		rb->read_index += data_len;
	} else {
		size_t first_part = rb->size - rb->read_index;
		if (first_part >= data_len) {
			memcpy (output, &rb->buffer [rb->read_index], data_len);
			rb->read_index += data_len;
		} else {
			memcpy (output, &rb->buffer [rb->read_index], first_part);
			size_t second_part = data_len - first_part;
			memcpy (&output [first_part], rb->buffer, second_part);
			rb->read_index = second_part;
		}
	}
	rb->length_read_index = (rb->length_read_index + 1) % rb->buffer_count;
	LOGD("%s length_read_index:%d length_write_index:%d write_index:%d read_index:%d available:%d data_len:%d\r\n",
		__func__, rb->length_read_index, rb->length_write_index, rb->write_index, rb->read_index, available, data_len);
	return data_len;
}

void data_buffer_clean(data_buffer_t *rb)
{
	size_t available = (rb->write_index >= rb->read_index)? (rb->write_index - rb->read_index) : (rb->size - rb->read_index + rb->write_index);
	LOGI("%s available:%d\r\n", __func__, available);
	memset(rb->buffer, 0, rb->size);
	memset(rb->length_buffer, 0, (rb->buffer_count * sizeof (size_t)));
	rb->read_index = 0;
	rb->write_index = 0;
	rb->length_read_index = 0;
	rb->length_write_index = 0;
	return;
}

bk_err_t websocket_event_send_msg(uint32_t event, uint32_t param)
{
	bk_err_t ret;
	wss_evt_msg_t msg;
	msg.event = event;
	msg.param = param;

	if (__get_beken_rtc() == NULL) {
		LOGE("%s, %d : %d rtc null, please check wss connection state\n", __func__, __LINE__, event);
		return BK_FAIL;
	}
	wss_evt_info_t *wss_evt_info = &(__get_beken_rtc()->wss_evt_info);
	ret = rtos_push_to_queue(&wss_evt_info->queue, &msg, BEKEN_NO_WAIT);
	if (BK_OK != ret)
	{
		LOGE("%s, %d : %d fail \n", __func__, __LINE__, event);
		return BK_FAIL;
	}

	return BK_OK;
}

static void wss_event_thread(beken_thread_arg_t data)
{
	int ret = BK_OK;
	wss_evt_info_t *wss_evt_info = data;
	while (1)
	{
		wss_evt_msg_t msg;

		ret = rtos_pop_from_queue(&wss_evt_info->queue, &msg, BEKEN_WAIT_FOREVER);
		if (ret == BK_OK)
		{
			switch (msg.event)
			{
				case WSS_EVT_SESSION_UPDATE:
					LOGI("hello from server\n");
					rtc_websocket_send_text(__get_beken_rtc(), (void *)(&dialog_info), BEKEN_RTC_SESSION_UPDATE);
					break;
				case WSS_EVT_SERVER_SESSION_UPDATED:
					LOGI("updated from server\n");
					break;
				case WSS_EVT_AUDIO_BUF_COMMIT:
					LOGI("audio buf commit\n");
					rtc_websocket_send_text(__get_beken_rtc(), (void *)(&dialog_info), BEKEN_RTC_INPUT_AUDIO_BUFFER_COMMIT);
					break;
				case WSS_EVT_RSP_CREATE:
					rtc_websocket_send_text(__get_beken_rtc(), (void *)(&dialog_info), BEKEN_RTC_RESPONSE_CREATE);
					break;
				case WSS_EVT_SEND_FC_FLAG:
					rtc_websocket_send_text(__get_beken_rtc(), NULL, BEKEN_RTC_SEND_FC_FLAG);
					break;
				case WSS_EVT_SUBTITLE_DISPLAY:
					LOGI("update text from server type:%s %s\n", ((text_info_t *)msg.param)->text_type ? "reply":"request",
						((text_info_t *)msg.param)->text_data);
#if CONFIG_SINGLE_SCREEN_FONT_DISPLAY
					if (lvgl_app_init_flag == 1) {
						media_app_lvgl_send_data((text_info_t *)msg.param);
					}
#endif
					if (((text_info_t *)msg.param)->text_data)
						os_free(((text_info_t *)msg.param)->text_data);
					if((text_info_t *)msg.param)
						os_free((text_info_t *)msg.param);
					break;
				case WSS_EVT_EXIT:
					LOGI("WSS_EVT_EXIT\n");
					goto exit;
			}
		}
	}
exit:
	if (wss_evt_info && wss_evt_info->queue) {
		LOGI("%s, deinit queue\r\n", __func__);
		ret = rtos_deinit_queue(&wss_evt_info->queue);
		wss_evt_info->queue = NULL;
	}
	LOGI("%s, exit\r\n", __func__);
	rtos_delete_thread(NULL);
}

int wss_event_init(wss_evt_info_t *wss_evt_info)
{
	int ret = BK_OK;

	os_memset(wss_evt_info, 0, sizeof(wss_evt_info_t));

	ret = rtos_init_queue(&wss_evt_info->queue,
                          "wss_event_queue",
                          sizeof(wss_evt_info_t),
                          15);

	if (ret != BK_OK)
	{
		LOGE("%s, init queue failed\r\n", __func__);
		return BK_FAIL;
	}

	ret = rtos_create_thread(&wss_evt_info->thread,
								BEKEN_DEFAULT_WORKER_PRIORITY - 1,
								"wss_evt_thread",
								(beken_thread_function_t)wss_event_thread,
								1024 * 4,
								(beken_thread_arg_t)wss_evt_info);

	if (ret != BK_OK)
	{
		LOGE("%s, init thread failed\r\n", __func__);
		rtos_deinit_queue(&wss_evt_info->thread);
		return BK_FAIL;
	}
	return BK_OK;
}

void wss_event_deinit(wss_evt_info_t *wss_evt_info)
{
	LOGI("wss_event_deinit\n");
	if (wss_evt_info && wss_evt_info->queue) {
		rtos_deinit_queue(&wss_evt_info->queue);
		wss_evt_info->queue = NULL;
	}
	if (wss_evt_info && wss_evt_info->thread) {
		rtos_delete_thread(&wss_evt_info->thread);
		wss_evt_info->thread = NULL;
	}
}

int data_buffer_available(data_buffer_t *rb)
{
	int diff = (rb->length_read_index > rb->length_write_index) ?
		(rb->length_read_index - rb->length_write_index) : (rb->buffer_count - rb->length_write_index + rb->length_read_index);
	return diff;
}

void data_check_fc_send_msg(rtc_session *session)
{
	int diff = data_buffer_available(session->ring_buffer);

	if ((diff > ((session->ring_buffer->buffer_count) * 1)/2) && !session->fc_is_acked) {
		websocket_event_send_msg(WSS_EVT_SEND_FC_FLAG, 0);
		session->fc_is_acked = 1;
	}
	else if ((diff <= 5) && session->fc_is_acked) {
		LOGE("%s fc msg may lost, available:%d\r\n", __func__, diff);
		websocket_event_send_msg(WSS_EVT_SEND_FC_FLAG, 0);
	}
}

void data_check(void *param)
{
	rtc_session *session = (rtc_session *) param;
	if(session == NULL) {
		LOGE("session null...\n");
		return;
	}
	uint8_t *packet = NULL;
	packet = os_zalloc(session->audio_info.dec_node_size + HEAD_SIZE_TOTAL);
	int size = 0;
	if (packet != NULL)
	{
		rtos_lock_mutex(&session->rtc_mutex);
		if (session->ring_buffer) {
#if CONFIG_BK_WSS_TRANS_NOPSRAM
			data_check_fc_send_msg(session);
#endif
			if ((size = data_buffer_read(session->ring_buffer, packet))
#if CONFIG_BK_WSS_TRANS_NOPSRAM
				&& session->playing_state
#endif
			) {
				cJSON *root = cJSON_Parse((char *)packet);
				if (root != NULL) {
					_rtc_websocket_audio_receive_text(root);
					cJSON_Delete(root);
				}
				else
				{
					_rtc_websocket_audio_receive_data(session, packet, size, session->rtc_channel_t->cb);
				}
				LOGD("data coming...\n");
			} else {
				LOGD("Buffer empty, waiting for data...\n");
			}
		}
		rtos_unlock_mutex(&session->rtc_mutex);
	}
	os_free(packet);
}

void data_start_timeout_check(uint32_t timeout, void *param)
{
	bk_err_t err = kNoErr;
	rtc_session *session = (rtc_session *) param;
	if(session == NULL) {
		LOGE("client null...\n");
		return;
	}
	LOGI("ring_data status timer start!!! dectype:%s\n", session->audio_info.decoding_type);
	err = rtos_init_timer(&session->data_read_tmr, timeout, (timer_handler_t)data_check, param);

	BK_ASSERT(kNoErr == err);
	err = rtos_start_timer(&session->data_read_tmr);
	BK_ASSERT(kNoErr == err);
	LOGI("ring_data status timer:%d\n", timeout);

	return;
}

void data_stop_timeout_check(beken_timer_t *data_read_tmr)
{
	if(!data_read_tmr->handle) {
		LOGE("data_read_tmr deinit already...\n");
		return;
	}

	if (rtos_is_timer_init(data_read_tmr)) {
		if (rtos_is_timer_running(data_read_tmr))
		{
			rtos_stop_timer(data_read_tmr);
		}
	rtos_deinit_timer(data_read_tmr);
	}
}

void wss_record_start(rtc_session *handle)
{
	if (handle)
		handle->recording_state = 1;
}

void wss_record_stop(rtc_session *handle)
{
	if (handle)
		handle->recording_state = 0;
}

int wss_record_work(rtc_session *handle)
{
	if (handle)
		return handle->recording_state;
	else
		return BK_FAIL;
}

void wss_play_start(rtc_session *handle)
{
	if (handle)
		handle->playing_state = 1;
}

void wss_play_stop(rtc_session *handle)
{
	if (handle)
		handle->playing_state = 0;
}

uint8 hnd_crc8(
    uint8 *pdata,   /* pointer to array of data to process */
    uint  nbytes,   /* number of input data bytes to process */
    uint8 crc   /* either CRC8_INIT_VALUE or previous return value */
)
{
	/* hard code the crc loop instead of using CRC_INNER_LOOP macro
	 * to avoid the undefined and unnecessary (uint8 >> 8) operation.
	 */
	while (nbytes-- > 0)
	{
		crc = crc8_table[(crc ^ *pdata++) & 0xff];
	}

	return crc;
}

uint32_t rtc_client_transmission_get_milliseconds(void)
{
	uint32_t time = 0;

#if CONFIG_ARCH_RISCV
	extern u64 riscv_get_mtimer(void);

	time = (riscv_get_mtimer() / 26000) & 0xFFFFFFFF;
#elif CONFIG_ARCH_CM33

	time = (bk_aon_rtc_get_us() / 1000) & 0xFFFFFFFF;
#endif

	return time;
}

void rtc_client_transmission_dealloc(db_channel_t *db_channel)
{

	if (db_channel->cbuf)
	{
		os_free(db_channel->cbuf);
		db_channel->cbuf = NULL;
	}

	if (db_channel->tbuf)
	{
		os_free(db_channel->tbuf);
		db_channel->tbuf = NULL;
	}
	os_memset(db_channel, 0, sizeof(db_channel_t));
	if (db_channel)
	{
		os_free(db_channel);
	}
}

db_channel_t *rtc_client_transmission_malloc(uint16_t max_rx_size, uint16_t max_tx_size)
{
	db_channel_t *db_channel = (db_channel_t *)os_malloc(sizeof(db_channel_t));

	if (db_channel == NULL)
	{
		LOGE("malloc db_channel failed\n");
		goto error;
	}

	os_memset(db_channel, 0, sizeof(db_channel_t));

	db_channel->cbuf = os_malloc(max_rx_size + sizeof(db_trans_head_t));

	if (db_channel->cbuf == NULL)
	{
		LOGE("malloc cache buffer failed\n");
		goto error;
	}

	db_channel->csize = max_rx_size + sizeof(db_trans_head_t);

	db_channel->tbuf = os_malloc(max_tx_size + sizeof(db_trans_head_t));
    //db_channel->tbuf = os_malloc(max_tx_size);

	if (db_channel->tbuf == NULL)
	{
		LOGE("malloc cache buffer failed\n");
		goto error;
	}

	db_channel->tsize = max_tx_size;

	LOGI("%s, %p, %p %d, %p %d\n", __func__, db_channel, db_channel->cbuf, db_channel->csize, db_channel->tbuf, db_channel->tsize);

	return db_channel;


error:

	if (db_channel->cbuf)
	{
		os_free(db_channel->cbuf);
		db_channel->cbuf = NULL;
	}

	if (db_channel)
	{
		os_free(db_channel);
		db_channel = NULL;
	}

	return db_channel;
}

void rtc_client_transmission_pack(db_channel_t *channel, uint8_t *data, uint32_t length)
{
	db_trans_head_t *head = channel->tbuf;

	/*
	*   Magic code  2 bytes
	*   Flags       2 bytes
	*   Timestamp   4 bytes
	*   Squence     2 bytes
	*   Length      2 bytes
	*   CRC         1 byte
	*   RESERVED    3 byte
	*/
	head->magic = CHECK_ENDIAN_UINT16(HEAD_MAGIC_CODE);
	head->flags = CHECK_ENDIAN_UINT16(HEAD_FLAGS_CRC);
	head->timestamp = CHECK_ENDIAN_UINT32(rtc_client_transmission_get_milliseconds());
	head->sequence = CHECK_ENDIAN_UINT16(++channel->sequence);
	head->length = CHECK_ENDIAN_UINT16(length);
	head->crc = hnd_crc8(data, length, CRC8_INIT_VALUE);
	head->reserved[0] = 0;
	head->reserved[1] = 0;
	head->reserved[2] = 0;
	LOGD("%s time: %u, len: %u, seq: %u, crc: %02X\n", __func__,
		head->timestamp, head->length, head->sequence, head->crc);

	os_memcpy(head->payload, data, length);
}

void rtc_client_transmission_unpack(db_channel_t *channel, uint8_t *data, uint32_t length, rtc_user_audio_rx_data_handle_cb cb)
{
	db_trans_head_t head, *ptr;
	uint8_t *p = data;
	uint32_t left = length;
	int cp_len = 0;

#ifdef DUMP_DEBUG
	static uint32_t count = 0;

	LOGD("DUMP DATA %u, size: %u\n", count++, length);

	rtc_client_hex_dump(data, length);
#else
	LOGD("recv unpack: %u\n", length);
#endif

	while (left != 0)
	{
		if (channel->ccount == 0)
		{
			if (left < HEAD_SIZE_TOTAL)
			{
				LOGE("left head size not enough: %d, ccount: %d\n", left, channel->ccount);
				os_memcpy(channel->cbuf + channel->ccount, p, left);
				channel->ccount += left;
				break;
			}

			ptr = (db_trans_head_t *)p;

			head.magic = CHECK_ENDIAN_UINT16(ptr->magic);

			if (head.magic == HEAD_MAGIC_CODE)
			{
				/*
				*   Magic code  2 bytes
				*   Flags       2 bytes
				*   Timestamp   4 bytes
				*   Squence     2 bytes
				*   Length      2 bytes
				*   CRC         1 byte
				*   RESERVED    3 byte
				*/
				if (CHECK_ENDIAN_UINT16(ptr->sequence) > 0 && (channel->last_seq + 1 != CHECK_ENDIAN_UINT16(ptr->sequence))) {
					LOGE("unexpected seq, last seq:%u, now head seq: %u, crc: %02X length:%d\n", channel->last_seq,
						CHECK_ENDIAN_UINT16(ptr->sequence), ptr->crc, length);
				}

				head.flags = CHECK_ENDIAN_UINT16(ptr->flags);
				head.timestamp = CHECK_ENDIAN_UINT32(ptr->timestamp);
				head.sequence = CHECK_ENDIAN_UINT16(ptr->sequence);
				channel->last_seq = head.sequence;
				head.length = CHECK_ENDIAN_UINT16(ptr->length);
				head.crc = ptr->crc;
				head.reserved[0] = ptr->reserved[0];
				head.reserved[1] = ptr->reserved[1];
				head.reserved[2] = ptr->reserved[2];
#ifdef DEBUG_HEAD
				LOGI("head size: %d, %d, flags: %04X\n", HEAD_SIZE_TOTAL, sizeof(db_trans_head_t), head.flags);
				LOGI("time: %u, len: %u, seq: %u, crc: %02X\n",
					head.timestamp, head.length, head.sequence, head.crc);
#endif
			LOGD("vaild head size: %d, %d, flags: %04X, %u, len: %u, seq: %u, crc: %02X\n", 
				HEAD_SIZE_TOTAL, sizeof(db_trans_head_t), head.flags,
				head.timestamp, head.length, head.sequence, head.crc);


			}
			else
			{
				LOGI("invaild head size: %d, %d, flags: %04X, %u, len: %u, seq: %u, crc: %02X magic:%X\n", 
					HEAD_SIZE_TOTAL, sizeof(db_trans_head_t), head.flags,
					head.timestamp, head.length, head.sequence, head.crc, head.magic);
				break;
			}

			if (left < head.length + HEAD_SIZE_TOTAL)
			{
				LOGE("left payload size not enough: %d, ccount: %d, pay len: %d\n", left, channel->ccount, head.length);
				os_memcpy(channel->cbuf + channel->ccount, p, left);
				channel->ccount += left;
				break;
			}

#ifdef DEBUG_CRC
			if (HEAD_FLAGS_CRC & head.flags)
			{

				uint8_t ret_crc = hnd_crc8(p + HEAD_SIZE_TOTAL, head.length, CRC8_INIT_VALUE);

				if (ret_crc != head.crc)
				{
					LOGI("check crc failed\n");
				}

				LOGD("CRC SRC: %02X,  CALC: %02X\n", head.crc, ret_crc);
			}
#endif

			if (cb)
			{
				cb(ptr->payload, head.length, NULL);
			}
			
			p += HEAD_SIZE_TOTAL + head.length;
			left -= HEAD_SIZE_TOTAL + head.length;
		}
		else
		{
			if (channel->ccount < HEAD_SIZE_TOTAL)
			{
				cp_len = HEAD_SIZE_TOTAL - channel->ccount;

				if (cp_len < 0)
				{
					//LOGE("cp_len error: %d at %d\n", cp_len, __LINE__);
					break;
				}


				if (left < cp_len)
				{
					os_memcpy(channel->cbuf + channel->ccount, p, left);
					channel->ccount += left;
					left = 0;
					//LOGE("cp_len head size not enough: %d, ccount: %d\n", cp_len, channel->ccount);
					break;
				}
				else
				{
					os_memcpy(channel->cbuf + channel->ccount, p, cp_len);
					channel->ccount += cp_len;
					p += cp_len;
					left -= cp_len;
				}
			}

			ptr = (db_trans_head_t *)channel->cbuf;

			head.magic = CHECK_ENDIAN_UINT32(ptr->magic);

			if (head.magic == HEAD_MAGIC_CODE)
			{
				/*
				*   Magic code  2 bytes
				*   Flags       2 bytes
				*   Timestamp   4 bytes
				*   Squence     2 bytes
				*   Length      2 bytes
				*   CRC         1 byte
				*   RESERVED    3 byte
				*/

				head.flags = CHECK_ENDIAN_UINT16(ptr->flags);
				head.timestamp = CHECK_ENDIAN_UINT32(ptr->timestamp);
				head.sequence = CHECK_ENDIAN_UINT16(ptr->sequence);
				head.length = CHECK_ENDIAN_UINT16(ptr->length);
				head.crc = ptr->crc;
				head.reserved[0] = ptr->reserved[0];
				head.reserved[1] = ptr->reserved[1];
				head.reserved[2] = ptr->reserved[2];

#ifdef DEBUG_HEAD
				LOGI("head size: %d, %d, flags: %04X\n", HEAD_SIZE_TOTAL, sizeof(db_trans_head_t), head.flags);
				LOGI("time: %u, len: %u, seq: %u, crc: %02X\n",
					head.timestamp, head.length, head.sequence, head.crc);
#endif
			}
			else
			{
				//LOGE("invaild cached data, %04X, %d\n", head.magic, __LINE__);
				rtc_client_hex_dump(channel->cbuf, channel->ccount);
				//TODO FIXME
				break;
			}

			if (channel->ccount < HEAD_SIZE_TOTAL + head.length)
			{
				cp_len = head.length + HEAD_SIZE_TOTAL - channel->ccount;

				if (cp_len < 0)
				{
					LOGE("cp_len error: %d at %d\n", cp_len, __LINE__);
					break;
				}

				if (left < cp_len)
				{
					os_memcpy(channel->cbuf + channel->ccount, p, left);
					channel->ccount += left;
					left = 0;
					///LOGE("cp_len payload size not enough: %d, ccount: %d\n", cp_len, channel->ccount);
					break;
				}
				else
				{
					os_memcpy(channel->cbuf + channel->ccount, p, cp_len);
					left -= cp_len;
					p += cp_len;
					channel->ccount += cp_len;
				}

#ifdef DEBUG_CRC
				if (HEAD_FLAGS_CRC & head.flags)
				{

					uint8_t ret_crc = hnd_crc8(channel->cbuf + HEAD_SIZE_TOTAL, head.length, CRC8_INIT_VALUE);

					if (ret_crc != head.crc)
					{
						LOGI("check crc failed\n");
					}

					LOGI("CRC SRC: %02X,  CALC: %02X\n", head.crc, ret_crc);
				}
#endif

				if (cb)
				{
					cb(ptr->payload, head.length, NULL);
				}
				//LOGI("cached: %d, left: %d\n", channel->ccount, left);

				channel->ccount = 0;
			}
			else
			{
				LOGE("invaild flow data\n");
				rtc_client_hex_dump(channel->cbuf, channel->ccount);
				//SHOULD NOT BE HERE
				//TODO FIMXME
				break;
			}
		}
	}
	//LOGI("next cached: %d\n", channel->ccount);
}

void _rtc_websocket_audio_receive_data(rtc_session *rtc_session, uint8 *data, uint32_t len, rtc_user_audio_rx_data_handle_cb cb)
{
	rtc_client_transmission_unpack(rtc_session->rtc_channel_t, data, len, cb);
}

void _rtc_websocket_audio_receive_text(cJSON *root)
{
	cJSON *type = cJSON_GetObjectItem(root, "type");
	if (type != NULL) {
		if ((os_strcmp(type->valuestring, "reply_text") == 0)
				|| (os_strcmp(type->valuestring, "request_text") == 0)) {
			text_info_t *info = os_zalloc(sizeof(text_info_t));
			info->text_type = (os_strcmp(type->valuestring, "request_text") == 0) ? 0:1;
			rtc_websocket_parse_text(info, root);
			LOGE("text: type:%s data:%s\n", info->text_type ? "reply":"request", info->text_data);
			websocket_event_send_msg(WSS_EVT_SUBTITLE_DISPLAY, (uint32_t)info);
		}
	}
}

void rtc_websocket_audio_receive_data(rtc_session *rtc_session, uint8 *data, uint32_t len)
{
	if (rtc_session->ab_buffer && (!fixed_data_buffer_write(rtc_session->ab_buffer, data, len))) {
		LOGE("Buffer full, dropping packet!\n");
	}
}

void rtc_websocket_rx_data_clean(void)
{
	rtc_session *rtc_sess = __get_beken_rtc();
	if (rtc_sess && rtc_sess->ring_buffer) {
		data_buffer_clean(rtc_sess->ring_buffer);
	}
}

void rtc_websocket_audio_receive_data_general(rtc_session *rtc_session, uint8 *data, uint32_t len)
{
	if (len > (rtc_session->audio_info.dec_node_size + HEAD_SIZE_TOTAL)) {
		LOGE("data too large, dropping packet!!!!! len:%d limit:%d\n", len, rtc_session->audio_info.dec_node_size);
		return;
	}
	if (rtc_session->ring_buffer) {
		data_buffer_write(rtc_session->ring_buffer, data, len);
	}
}

void rtc_websocket_audio_receive_text(rtc_session *rtc_session, uint8 *data, uint32_t len)
{
	if (len > rtc_session->audio_info.dec_node_size) {
		LOGE("data too large, dropping packet!!!!! len:%d limit:%d\n", len, rtc_session->audio_info.dec_node_size);
		return;
	}
	if (rtc_session->ring_buffer) {
		data_buffer_write(rtc_session->ring_buffer, data, len);
	}
}

int rtc_websocket_audio_send_packed_data(rtc_session *rtc_session, uint8_t *data_ptr, size_t data_len)
{
	int ret = BK_OK;

	transport bk_rtc_ws = rtc_session->bk_rtc_client;
	db_channel_t *rtc_channel_t = rtc_session->rtc_channel_t;

	rtc_client_transmission_pack(rtc_channel_t, data_ptr, data_len);
	ret = websocket_client_send_binary(bk_rtc_ws, (const char *)rtc_channel_t->tbuf, (data_len + HEAD_SIZE_TOTAL), 10*1000);
	return ret;
}

int rtc_websocket_audio_send_data(uint8_t *data_ptr, size_t data_len)
{
	rtc_session *rtc_session = __get_beken_rtc();
	int ret = BK_OK;

	if (!rtc_session) {
		LOGE("rtc_session need to be init\r\n");
		return -1;
	}
	if (rtc_session->disconnecting_state) {
		LOGE("wss disconnecting, not send data\r\n");
		return -1;
	}
#if CONFIG_BK_WSS_TRANS_NOPSRAM
	if (!rtc_session->recording_state) {
		LOGD("wss waiting for recording state, not send data\r\n");
		return 0;
	}
#endif

	rtos_lock_mutex(&rtc_session->rtc_mutex);

	if (os_strncmp("pcm", rtc_session->audio_info.encoding_type, 3) == 0) {
		// Cache data until having 4 packets
		if (rtc_session->send_cache_count < 4) {
			os_memcpy(rtc_session->send_cache_buffer + rtc_session->send_cache_count * data_len, data_ptr, data_len);
			rtc_session->send_cache_count++;
		}

		// Send combined packet when having 4 packets
		if (rtc_session->send_cache_count == 4) {
			ret = rtc_websocket_audio_send_packed_data(rtc_session, rtc_session->send_cache_buffer, rtc_session->send_cache_size);
			rtc_session->send_cache_count = 0;
		}
	}
	else {
		ret = rtc_websocket_audio_send_packed_data(rtc_session, data_ptr, data_len);
	}
	if (!rtc_session || !rtc_session->rtc_mutex) {
		LOGE("websocket send binary fail! err:%d\r\n", ret);
		return ret;
	}
	rtos_unlock_mutex(&rtc_session->rtc_mutex);
	return ret;
}

int rtc_websocket_send_text(rtc_session *rtc_session, void *str, enum MsgType msgtype) {
	if (rtc_session == NULL) {
		LOGE("Invalid arguments\r\n");
		return -1;
	}
	LOGD("add extra str: %s\r\n", str ? "yes":"no need");
	char *buf = NULL;
	if (NULL == (buf = (char *)os_zalloc(BEKEN_RTC_TXT_SIZE))) {
		LOGE("alloc user context fail\r\n");
		return -1;
	}
	int n = 0;
	switch (msgtype) {
#if !CONFIG_BK_WSS_TRANS_NOPSRAM
		case BEKEN_RTC_SEND_HELLO:
			n = os_snprintf(buf, BEKEN_RTC_TXT_SIZE,
				"{\"type\":\"hello\",\"config\":{\"version\": 2,\"audio\":{\"to_server\":{\"format\":\"%s\", \"sample_rate\":%u, \"channels\":1, \"frame_duration\":%u}, \"from_server\":{\"format\":\"%s\", \"sample_rate\":%u, \"channels\":1, \"frame_duration\":%u}}}}",
				((audio_info_t *)str)->encoding_type, ((audio_info_t *)str)->adc_samp_rate, ((audio_info_t *)str)->enc_samp_interval,
						((audio_info_t *)str)->decoding_type, ((audio_info_t *)str)->dac_samp_rate, ((audio_info_t *)str)->dec_samp_interval);
			LOGI("Sending: %s\r\n", buf);
			websocket_client_send_text(rtc_session->bk_rtc_client, buf, n, 10*1000);
			break;
#else
		case BEKEN_RTC_SEND_HELLO:
			n = snprintf(buf, BEKEN_RTC_TXT_SIZE, 
                        "{\"type\":\"hello\",\"interact_mode\":4}");
            LOGI("Hello Sending: %s\r\n", buf);
            websocket_client_send_text(rtc_session->bk_rtc_client, buf, n, 10*1000);
            break;
        case BEKEN_RTC_SESSION_UPDATE:
            n = snprintf(buf, BEKEN_RTC_TXT_SIZE, 
                        "{"
                        "\"type\":\"session.update\", "
                        "\"session\":{"
                        "\"devId\":\"%s\", "
                        "\"nfcId\":\"%s\", "
                        "\"input_audio_format\":\"%s\", "
                        "\"input_audio_rate\":%d, "
                        "\"input_audio_duration\":%d, "
                        "\"output_audio_format\":\"%s\", "
                        "\"output_audio_rate\":%d, "
                        "\"cloud_vad\":%d, "
                        "\"source\":\"%s\", "
                        "\"pack_size\":%d "
                        "}"
                        "}",
                        ((dialog_session_t *)str)->devId, ((dialog_session_t *)str)->nfcId,
                        ((dialog_session_t *)str)->input_audio_format, ((dialog_session_t *)str)->input_audio_rate,
                        rtc_session->audio_info.enc_samp_interval,
                        ((dialog_session_t *)str)->output_audio_format, ((dialog_session_t *)str)->output_audio_rate,
                        ((dialog_session_t *)str)->cloud_vad, ((dialog_session_t *)str)->source,
                        rtc_session->ring_buffer->buffer_count/2);
            LOGI("Session_Update: %s\r\n", buf);
            websocket_client_send_text(rtc_session->bk_rtc_client, buf, n, 10*1000);
            break;
        case BEKEN_RTC_INPUT_AUDIO_BUFFER_APPEND:
            n = snprintf(buf, BEKEN_RTC_TXT_SIZE, "{\"type\":\"input_audio_buffer.append\"}");
            LOGI("Audio_Buf_Commit: %s\r\n", buf);
            websocket_client_send_text(rtc_session->bk_rtc_client, buf, n, 10*1000);
            app_event_send_msg(APP_EVT_ASR_WAKEUP, 0);
            break;
        case BEKEN_RTC_INPUT_AUDIO_BUFFER_CLEAR:
            n = snprintf(buf, BEKEN_RTC_TXT_SIZE, "{\"type\":\"input_audio_buffer.clear\"}");
            LOGI("Audio_Buf_Clear: %s\r\n", buf);
            websocket_client_send_text(rtc_session->bk_rtc_client, buf, n, 10*1000);
            break;
        case BEKEN_RTC_INPUT_AUDIO_BUFFER_COMMIT:
            n = snprintf(buf, BEKEN_RTC_TXT_SIZE, "{\"type\":\"input_audio_buffer.commit\"}");
            LOGI("Audio_Buf_Commit: %s\r\n", buf);
            websocket_client_send_text(rtc_session->bk_rtc_client, buf, n, 10*1000);
            break;
        case BEKEN_RTC_RESPONSE_CREATE:
            n = snprintf(buf, BEKEN_RTC_TXT_SIZE, 
                        "{\"type\":\"response.create\",\"data_type\":\"binary\"}");
            LOGI("Response_Create: %s\r\n", buf);
            websocket_client_send_text(rtc_session->bk_rtc_client, buf, n, 10*1000);
            break;
         case BEKEN_RTC_ERROR:
            n = snprintf(buf, BEKEN_RTC_TXT_SIZE, "{\"type\":\"error\", \"error\":\"%s\"}", ((error_info_t *)str)->error_desc);
            LOGI("Error: %s\r\n", buf);
            websocket_client_send_text(rtc_session->bk_rtc_client, buf, n, 10*1000);
            break;
         case BEKEN_RTC_SEND_FC_FLAG:
            n = snprintf(buf, BEKEN_RTC_TXT_SIZE, "{\"type\":\"flow_control\", \"flag\":\"idle\"}");
            LOGI("Send_Fc: %s\r\n", buf);
            websocket_client_send_text(rtc_session->bk_rtc_client, buf, n, 10*1000);
            break;
#endif
		default:
			LOGI("Unsupported message type");
			return -1;
	}
	if(buf)
		free(buf);
	return n;  // Returns the number of bytes sent
}

int rtc_websocket_parse_hello(cJSON *root) {
	cJSON *code = cJSON_GetObjectItem(root, "code");
	cJSON *msg = cJSON_GetObjectItem(root, "msg");

	if (code == NULL || msg == NULL) {
		LOGE("Error: Missing required fields in hello.\n");
		return -1;
	}

	LOGE("Parsing hello_response:\n");
	LOGE("  code: %d\n", code->valueint);
	LOGE("  msg: %s\n", msg->valuestring);
	return code->valueint;
}

void rtc_websocket_parse_request_text(text_info_t *info, cJSON *root) {
	cJSON *text = cJSON_GetObjectItem(root, "user_text");

	if (text == NULL) {
		LOGE("Error: Missing required fields in user_text.\n");
		return;
	}

	LOGI("Parsing text: %s\n", text->valuestring);
	info->text_data = text->valuestring;
}

void rtc_websocket_parse_text(text_info_t *info, cJSON *root) {
	cJSON *text = cJSON_GetObjectItem(root, "text");

	if (text == NULL) {
		LOGE("Error: Missing required fields in request/reply text.\n");
		return;
	}

	LOGD("Parsing text: %s\n", text->valuestring);
	info->text_data = os_strdup(text->valuestring);
}

void rtc_fill_audio_info(audio_info_t *info, char *enctype, char *dectype, uint32_t adc_rate, uint32_t dac_rate, uint32_t enc_ms, uint32_t dec_ms, uint32_t enc_size, uint32_t dec_size)
{
	memset(info, 0, sizeof(audio_info_t));
	os_strcpy(info->encoding_type, enctype);
    os_strcpy(info->decoding_type, dectype);
	info->adc_samp_rate = adc_rate;
	info->dac_samp_rate = dac_rate;
	info->enc_samp_interval = enc_ms;
	info->dec_samp_interval = dec_ms;
	info->enc_node_size = enc_size;
    info->dec_node_size = dec_size;
}

void rtc_get_dialog_info(aud_intf_voc_setup_t *aud_intf_voc_setup, char *encoder_name, char *decoder_name)
{
    uint8_t devId_mac[6];
    char devId_mac_str[18];

    bk_wifi_sta_get_mac(devId_mac);
    os_snprintf(devId_mac_str, 18, "%02x:%02x:%02x:%02x:%02x:%02x", devId_mac[0], devId_mac[1], devId_mac[2], devId_mac[3], devId_mac[4], devId_mac[5]);
    rtc_fill_dialog_info(&dialog_info, devId_mac_str, "", decoder_name, aud_intf_voc_setup->aud_codec_setup_input.dac_samp_rate, 
                         encoder_name, aud_intf_voc_setup->aud_codec_setup_input.adc_samp_rate, 0, "BKR1");
}
/* For session.updated message */
void parse_session_updated(text_info_t *info, cJSON *root) {
    cJSON *session = cJSON_GetObjectItem(root, "session");
    if (!session) {
        LOGE("Missing session object\n");
        return;
    }
    
    cJSON *devId = cJSON_GetObjectItem(session, "devId");
    cJSON *nfcId = cJSON_GetObjectItem(session, "nfcId");
    cJSON *input_audio_format = cJSON_GetObjectItem(session, "input_audio_format");
    cJSON *input_audio_rate = cJSON_GetObjectItem(session, "input_audio_rate");
    cJSON *output_audio_format = cJSON_GetObjectItem(session, "output_audio_format");
    cJSON *output_audio_rate = cJSON_GetObjectItem(session, "output_audio_rate");

    if (devId == NULL || nfcId == NULL || input_audio_format == NULL || input_audio_rate == NULL ||
        output_audio_format == NULL || output_audio_rate == NULL) {
        LOGE("Error: Missing required fields in 'session' object.\n");
        return;
    }

    LOGI("Session updated: devId=%s, nfcId=%s\n", devId->valuestring, nfcId->valuestring);
    // Store relevant information in info struct
    info->devId = devId->valuestring;
    info->nfcId = nfcId->valuestring;
    info->input_audio_format = input_audio_format->valuestring;
    info->input_audio_rate = input_audio_rate->valueint;
    info->output_audio_format = output_audio_format->valuestring;
    info->output_audio_rate = output_audio_rate->valueint;
}

/* For input_audio_buffer.committed message */
void parse_audio_committed(text_info_t *info, cJSON *root) {
    LOGI("ASR has received and started processing audio\n");
    //ToDo:add some flag?
}

/* For response.created message */
void parse_text_response(text_info_t *info, cJSON *root) {
    rtc_websocket_parse_text(info, root);
    //ToDo
}

/* For binary audio frames */
void parse_audio_frame(text_info_t *info, const uint8_t *data, size_t len) {
    LOGI("Received audio frame of size %zu\n", len);
    //ToDo
}

/* For response.audio.done message */
void parse_audio_done(text_info_t *info, cJSON *root) {
    cJSON *mode = cJSON_GetObjectItem(root, "mode");
    if (mode && strcmp(mode->valuestring, "mutil-chat") == 0) {
        LOGD("Multi-turn conversation mode activated\n");
        info->multi_turn_mode = true;
        //ToDo
    } else {
        LOGD("Audio playback completed\n");
        info->playback_complete = true;
    }
}

void rtc_fill_dialog_info(dialog_session_t *dialog_info, char *devId, char *nfcId,
                                char *input_audio_format, uint32_t input_audio_rate,
                                char *output_audio_format, uint32_t output_audio_rate, uint32_t cloud_vad, char *source)
{
    memset(dialog_info, 0, sizeof(dialog_session_t));
    if (devId) {
        os_strcpy(dialog_info->devId, devId);
    }

    if (nfcId) {
        os_strcpy(dialog_info->nfcId, nfcId);
    }

    if (input_audio_format) {
        os_strcpy(dialog_info->input_audio_format, input_audio_format);
    }

    dialog_info->input_audio_rate = input_audio_rate;

    if (output_audio_format) {
        os_strcpy(dialog_info->output_audio_format, output_audio_format);
    }

    dialog_info->output_audio_rate = output_audio_rate;
    dialog_info->cloud_vad = cloud_vad;

    if (source) {
        os_strcpy(dialog_info->source, source);
    }
}

rtc_session *rtc_websocket_create(websocket_client_input_t *websocket_cfg, rtc_user_audio_rx_data_handle_cb cb, audio_info_t *info)
{
	rtc_session *rtc_sess = (rtc_session *)os_malloc(sizeof(rtc_session));
	memset(rtc_sess, 0, sizeof(rtc_session));
	int ret = 0;
	int max_count = 0;
	rtc_sess->bk_rtc_client = websocket_client_init(websocket_cfg);
	if(websocket_client_start(rtc_sess->bk_rtc_client)) {
		LOGE("%s fail\r\n", __func__);
		goto fail;
	}

	rtc_fill_audio_info(&rtc_sess->audio_info, info->encoding_type, info->decoding_type, info->adc_samp_rate, info->dac_samp_rate,
		info->enc_samp_interval, info->dec_samp_interval, info->enc_node_size, info->dec_node_size);
	if (rtc_sess->audio_info.dec_node_size) {
		max_count = WSS_AUDIO_BUFFER_SIZE / (rtc_sess->audio_info.dec_node_size + HEAD_SIZE_TOTAL);
		LOGI("rtc ws create successfully. audio enctype:%s dectype:%s adc_rate:%d dac_rate:%d enc:%d dec:%d encsize:%d decsize:%d max_count:%d\n", info->encoding_type,
			info->decoding_type, rtc_sess->audio_info.adc_samp_rate, rtc_sess->audio_info.dac_samp_rate, rtc_sess->audio_info.enc_samp_interval,
			rtc_sess->audio_info.dec_samp_interval, rtc_sess->audio_info.enc_node_size, rtc_sess->audio_info.dec_node_size, max_count);
	} else {
		LOGE("audio info error\n");
		goto fail;
	}

	// Initialize send cache for 4-packet combination
	rtc_sess->send_cache_size = rtc_sess->audio_info.enc_node_size * 4;
	rtc_sess->send_cache_buffer = (uint8_t *)os_malloc(rtc_sess->send_cache_size);
	if (!rtc_sess->send_cache_buffer) {
		LOGE("Failed to allocate memory for send cache buffer\n");
		goto fail;
	}
	rtc_sess->send_cache_count = 0;

	if (os_strncmp("pcm", rtc_sess->audio_info.encoding_type, 3) == 0)
		rtc_sess->rtc_channel_t = rtc_client_transmission_malloc(rtc_sess->audio_info.dec_node_size, rtc_sess->send_cache_size);
	else
		rtc_sess->rtc_channel_t = rtc_client_transmission_malloc(rtc_sess->audio_info.dec_node_size, rtc_sess->audio_info.enc_node_size);
	if (rtc_sess->rtc_channel_t == NULL)
	{
		LOGE("rtc_channel_t malloc failed\n");
		goto fail;
	}
	rtc_sess->rtc_channel_t->cb = cb;

	ret = rtos_init_mutex(&rtc_sess->rtc_mutex);
	if (ret != BK_OK)
	{
		LOGE("rtos_init_mutex failed\n");
		goto fail;
	}

    if (info->decoding_type) {
		rtc_sess->ring_buffer = data_buffer_init(((rtc_sess->audio_info.dec_node_size + HEAD_SIZE_TOTAL) * max_count), max_count);
		if (rtc_sess->ring_buffer == NULL)
    	{
			BK_LOGE(TAG, "%s, %d, data_buffer_init fail\n", __func__, __LINE__);
			goto fail;
    	}
    }

	if (wss_event_init(&(rtc_sess->wss_evt_info))) {
		LOGE("wss_event_init failed\n");
		goto fail;
	}

	data_start_timeout_check(rtc_sess->audio_info.dec_samp_interval, (void *)rtc_sess);
    return rtc_sess;

fail:
	if (rtc_sess->send_cache_buffer) {
		os_free(rtc_sess->send_cache_buffer);
		rtc_sess->send_cache_buffer = NULL;
	}
	if (rtc_sess->rtc_mutex)
	{
		rtos_deinit_mutex(&rtc_sess->rtc_mutex);
		rtc_sess->rtc_mutex == NULL;
	}
	if (rtc_sess->rtc_channel_t)
	{
		rtc_client_transmission_dealloc(rtc_sess->rtc_channel_t);
		rtc_sess->rtc_channel_t == NULL;
	}
	if (rtc_sess->bk_rtc_client) {
		websocket_client_destroy((transport)rtc_sess->bk_rtc_client);
		rtc_sess->bk_rtc_client = NULL;
	}
	if (rtc_sess) {
		os_free(rtc_sess);
		rtc_sess = NULL;
	}
	return NULL;
}

bk_err_t rtc_websocket_stop(rtc_session *rtc_session)
{
	if(rtc_session == NULL) {
		BK_LOGE(TAG, "session aleady null\r\n");
		return BK_FAIL;
	}
	wss_event_deinit(&rtc_session->wss_evt_info);
	data_stop_timeout_check(&rtc_session->data_read_tmr);

	data_buffer_deinit(rtc_session->ring_buffer);
	rtc_session->ring_buffer = NULL;

	if(rtc_session->bk_rtc_client) {
		LOGE("%s stop websocket client\r\n", __func__);
		rtc_session->disconnecting_state++;
		rtc_session->bk_rtc_client->ws_event_handler = NULL;
		websocket_client_destroy((transport)rtc_session->bk_rtc_client);
		rtc_session->bk_rtc_client = NULL;
	} else {
		LOGE("%s, already stop return\r\n", __func__);
	}
	if (rtc_session->rtc_channel_t)
	{
		rtc_client_transmission_dealloc(rtc_session->rtc_channel_t);
		rtc_session->rtc_channel_t = NULL;
	}
	if (rtc_session->send_cache_buffer) {
		os_free(rtc_session->send_cache_buffer);
		rtc_session->send_cache_buffer = NULL;
	}

	rtos_deinit_mutex(&rtc_session->rtc_mutex);
	rtc_session->rtc_mutex = NULL;

	if (rtc_session) {
		os_free(rtc_session);
	}
	return BK_OK;
}

