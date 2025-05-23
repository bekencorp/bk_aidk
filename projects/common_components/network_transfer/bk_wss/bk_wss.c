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


#define TAG "beken_rtc"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)
void _rtc_websocket_audio_receive_data(rtc_session *rtc_session, uint8 *data, uint32_t len, rtc_user_audio_rx_data_handle_cb cb);

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

#define WSS_AUDIO_BUFFER_SIZE (680*1024)

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
		BK_LOGE(TAG, "malloc ab fail\n");
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
		BK_LOGE(TAG, "malloc data buffer fail\n");
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
		BK_LOGE(TAG, "buffer deinit already\n");
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
			BK_LOGD(TAG, "data coming...\n");
		} else {
			BK_LOGD(TAG, "Buffer empty, waiting for data...\n");
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
		BK_LOGE(TAG, "malloc rb fail\n");
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
		BK_LOGE(TAG, "malloc buffer_size fail\n");
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
		BK_LOGE(TAG, "malloc length_buffer fail\n");
		os_free(rb);
		os_free(rb->buffer);
		return NULL;
	}
	memset((size_t*)rb->length_buffer, 0, length_buffer_size * sizeof (size_t));
	rb->length_read_index = 0;
	rb->length_write_index = 0;
	rb->buffer_count = length_buffer_size;
	BK_LOGE(TAG, "ringbuffer size:%d ringbuffer max count:%d\n", buffer_size, length_buffer_size);
	return rb;
}

void data_buffer_deinit(data_buffer_t *rb) {

	if (rb == NULL)
	{
		BK_LOGE(TAG, "buffer deinit already\n");
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
		BK_LOGE(TAG, "%s, write buffer fail length_write_index:%d write_index:%d read_index:%d data_len:%d\r\n",
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
	BK_LOGD(TAG, "%s length_read_index:%d length_write_index:%d write_index:%d read_index:%d available:%d data_len:%d\r\n",
		__func__, rb->length_read_index, rb->length_write_index, rb->write_index, rb->read_index, available, data_len);
	return data_len;
}

void data_check(void *param)
{
	rtc_session *session = (rtc_session *) param;
	if(session == NULL) {
		BK_LOGE(TAG, "session null...\n");
		return;
	}
	uint8_t *packet = NULL;
	packet = os_zalloc(session->audio_info.dec_node_size + HEAD_SIZE_TOTAL);
	int size = 0;
	if (packet != NULL)
	{
		rtos_lock_mutex(&session->rtc_mutex);
		if (session->opus_buffer && (size = data_buffer_read(session->opus_buffer, packet)) && session) {
			_rtc_websocket_audio_receive_data(session, packet, size, session->rtc_channel_t->cb);
			BK_LOGD(TAG, "data coming...\n");
		} else {
			BK_LOGD(TAG, "Buffer empty, waiting for data...\n");
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
		BK_LOGE(TAG, "client null...\n");
		return;
	}
	BK_LOGI(TAG,"ring_data status timer start!!! dectype:%s\n", session->audio_info.decoding_type);
    if (session->audio_info.decoding_type && os_strcmp(session->audio_info.decoding_type, "opus") == 0)
	    err = rtos_init_timer(&session->data_read_tmr, timeout, (timer_handler_t)data_check, param);
    else
	    err = rtos_init_timer(&session->data_read_tmr, timeout, (timer_handler_t)fixed_data_check, param);

	BK_ASSERT(kNoErr == err);
	err = rtos_start_timer(&session->data_read_tmr);
	BK_ASSERT(kNoErr == err);
	BK_LOGI(TAG,"ring_data status timer:%d\n", timeout);

	return;
}

void data_stop_timeout_check(beken_timer_t *data_read_tmr)
{
	if(!data_read_tmr->handle) {
		BK_LOGE(TAG, "data_read_tmr deinit already...\n");
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

void rtc_websocket_audio_receive_data(rtc_session *rtc_session, uint8 *data, uint32_t len)
{
	if (rtc_session->ab_buffer && (!fixed_data_buffer_write(rtc_session->ab_buffer, data, len))) {
		BK_LOGE(TAG, "Buffer full, dropping packet!\n");
	}
}

void rtc_websocket_audio_receive_data_opus(rtc_session *rtc_session, uint8 *data, uint32_t len)
{
	if (len > rtc_session->audio_info.dec_node_size) {
		BK_LOGE(TAG, "data too large, dropping packet!!!!! len:%d limit:%d\n", len, rtc_session->audio_info.dec_node_size);
		return;
	}
	if (rtc_session->opus_buffer) {
		data_buffer_write(rtc_session->opus_buffer, data, len);
	}
}

int rtc_websocket_audio_send_data(uint8_t *data_ptr, size_t data_len)
{
	rtc_session *rtc_session = __get_beken_rtc();

	if (!rtc_session) {
		BK_LOGE("WebSocket", "rtc_session need to be init\r\n");
		return -1;
	}
	rtos_lock_mutex(&rtc_session->rtc_mutex);
	transport bk_rtc_ws = rtc_session->bk_rtc_client;
	db_channel_t *rtc_channel_t = rtc_session->rtc_channel_t;

	rtc_client_transmission_pack(rtc_channel_t, data_ptr, data_len);
	int ret = websocket_client_send_binary(bk_rtc_ws, (const char *)rtc_channel_t->tbuf, (data_len + HEAD_SIZE_TOTAL), 10*1000);
	rtos_unlock_mutex(&rtc_session->rtc_mutex);
	return ret;
}

int rtc_websocket_send_text(transport web_socket, void *str, enum MsgType msgtype) {
	if (web_socket == NULL) {
		BK_LOGE("WebSocket", "Invalid arguments\r\n");
		return -1;
	}
	BK_LOGE("WebSocket", "add extra str: %s\r\n", str ? "yes":"no need");
	char *buf = NULL;
	if (NULL == (buf = (char *)os_zalloc(BEKEN_RTC_TXT_SIZE))) {
		BK_LOGE("WebSocket", "alloc user context fail\r\n");
		return -1;
	}
	int n = 0;
	switch (msgtype) {
		case BEKEN_RTC_SEND_HELLO:
			n = os_snprintf(buf, BEKEN_RTC_TXT_SIZE,
				"{\"type\":\"hello\",\"config\":{\"version\": 2,\"audio\":{\"to_server\":{\"format\":\"%s\", \"sample_rate\":%u, \"channels\":1, \"frame_duration\":%u}, \"from_server\":{\"format\":\"%s\", \"sample_rate\":%u, \"channels\":1, \"frame_duration\":%u}}}}",
				((audio_info_t *)str)->encoding_type, ((audio_info_t *)str)->adc_samp_rate, ((audio_info_t *)str)->enc_samp_interval,
						((audio_info_t *)str)->decoding_type, ((audio_info_t *)str)->dac_samp_rate, ((audio_info_t *)str)->dec_samp_interval);
			BK_LOGE("WebSocket", "Sending: %s\r\n", buf);
			websocket_client_send_text(web_socket, buf, n, 10*1000);
			break;
		default:
			BK_LOGE("WebSocket", "Unsupported message type");
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

void rtc_websocket_parse_text(text_info_t *info, cJSON *root) {
	cJSON *text = cJSON_GetObjectItem(root, "text");

	if (text == NULL) {
		LOGE("Error: Missing required fields in request/reply text.\n");
		return;
	}

	LOGD("Parsing text: %s\n", text->valuestring);
	info->text_data = text->valuestring;
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

void rtc_get_aud_inft_info(aud_intf_voc_setup_t *aud_intf_voc_setup, char *encoder_name, char *decoder_name)
{
    extern audio_info_t audio_info;
    rtc_fill_audio_info(&audio_info, 
                        encoder_name, 
                        decoder_name, 
                        aud_intf_voc_setup->aud_codec_setup_input.adc_samp_rate,
                        aud_intf_voc_setup->aud_codec_setup_input.dac_samp_rate,
                        aud_intf_voc_setup->aud_codec_setup_input.enc_frame_len_in_ms, 
                        aud_intf_voc_setup->aud_codec_setup_input.dec_frame_len_in_ms,
                        bk_aud_get_enc_output_size_in_byte(), 
                        bk_aud_get_dec_input_size_in_byte());

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
		LOGI("rtc ws create successfully. audio enctype:%s dectype:%s adc_rate:%d dac_rate:%d enc:%d dec:%d size:%d max_count:%d\n", info->encoding_type,
			info->decoding_type, rtc_sess->audio_info.adc_samp_rate, rtc_sess->audio_info.dac_samp_rate, rtc_sess->audio_info.enc_samp_interval,
			rtc_sess->audio_info.dec_samp_interval, rtc_sess->audio_info.enc_node_size, rtc_sess->audio_info.dec_node_size, max_count);
	} else {
		LOGE("audio info error\n");
		goto fail;
	}

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

    if (info->decoding_type && strcmp(info->decoding_type, "opus") == 0) {
        	rtc_sess->opus_buffer = data_buffer_init(((rtc_sess->audio_info.dec_node_size + HEAD_SIZE_TOTAL) * max_count), max_count);
        	if (rtc_sess->opus_buffer == NULL)
        	{
        		BK_LOGE(TAG, "%s, %d, data_buffer_init fail\n", __func__, __LINE__);
        		goto fail;
        	}
    }
    else {
	    rtc_sess->ab_buffer = fixed_data_buffer_init(((rtc_sess->audio_info.dec_node_size + HEAD_SIZE_TOTAL) * max_count));
    	if (rtc_sess->ab_buffer == NULL)
    	{
    		BK_LOGE(TAG, "%s, %d, fixed_data_buffer_init fail\n", __func__, __LINE__);
    		goto fail;
    	}
    }
	data_start_timeout_check(rtc_sess->audio_info.dec_samp_interval, (void *)rtc_sess);
    return rtc_sess;

fail:
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

	data_stop_timeout_check(&rtc_session->data_read_tmr);

	data_buffer_deinit(rtc_session->opus_buffer);
	rtc_session->opus_buffer = NULL;

	fixed_data_buffer_deinit(rtc_session->ab_buffer);
	rtc_session->ab_buffer = NULL;

	if(rtc_session->bk_rtc_client) {
		LOGE("%s stop websocket client\r\n", __func__);
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
	rtos_deinit_mutex(&rtc_session->rtc_mutex);
	rtc_session->rtc_mutex == NULL;
	if (rtc_session) {
		os_free(rtc_session);
	}

	return BK_OK;
}

