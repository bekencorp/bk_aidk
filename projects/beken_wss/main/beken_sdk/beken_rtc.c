#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#include "beken_rtc.h"
#include "beken_config.h"
#if CONFIG_ARCH_CM33
#include <driver/aon_rtc.h>
#endif
#include "bk_genie_comm.h"


#define TAG "beken_rtc"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

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

void rtc_client_hex_dump(uint8_t *data, uint32_t length)
{
	for (int i = 0; i < 12; i++)
	{
		os_printf("%02X ", data[i]);

		if ((i + 1) % 20 == 0)
		{
			os_printf("\n");
		}
	}
	os_printf("\n");
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
	db_trans_head_t head, *ptr, *ptr1;
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
	ptr1 = (db_trans_head_t *)p;
	LOGD("recv head seq: %u, crc: %02X magic:%X length:%d\n", 
		CHECK_ENDIAN_UINT16(ptr1->sequence), ptr1->crc, CHECK_ENDIAN_UINT16(ptr1->magic), length);

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

void rtc_websocket_audio_receive_data(rtc_session *rtc_session, uint8 *data, uint32_t len, rtc_user_audio_rx_data_handle_cb cb)
{
	rtos_lock_mutex(&rtc_session->rtc_mutex);
    rtc_client_transmission_unpack(rtc_session->rtc_channel_t, data, len, cb);
	rtos_unlock_mutex(&rtc_session->rtc_mutex);
}

int rtc_websocket_audio_send_data(rtc_session *rtc_session, uint8_t *data_ptr, size_t data_len)
{
	rtos_lock_mutex(&rtc_session->rtc_mutex);
	transport bk_rtc_ws = rtc_session->bk_rtc_client;
	db_channel_t *rtc_channel_t = rtc_session->rtc_channel_t;

	rtc_client_transmission_pack(rtc_channel_t, data_ptr, data_len);
	websocket_client_send_binary(bk_rtc_ws, (const char *)rtc_channel_t->tbuf, (data_len + HEAD_SIZE_TOTAL), 10*1000);
	rtos_unlock_mutex(&rtc_session->rtc_mutex);
	return BK_OK;
}

int rtc_websocket_send_text(transport web_socket, char *str, enum MsgType msgtype) {
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
			n = snprintf(buf, BEKEN_RTC_TXT_SIZE, 
						"{\"type\":\"hello\",\"version\": 1,\"transport\":\"websocket\",\"audio_params\":{\"format\":\"opus\", \"sample_rate\":16000, \"channels\":1, \"frame_duration\":%d}}",
						100);
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

rtc_session *rtc_websocket_create(websocket_client_input_t *websocket_cfg)
{
	rtc_session *rtc_sess = (rtc_session *)os_malloc(sizeof(rtc_session));
	memset(rtc_sess, 0, sizeof(rtc_session));

	rtc_sess->bk_rtc_client = websocket_client_init(websocket_cfg);
	if(websocket_client_start(rtc_sess->bk_rtc_client)) {
		LOGE("%s fail\r\n", __func__);
		goto fail;
	}
    LOGI("rtc create successfully.\n");

	rtc_sess->rtc_channel_t = rtc_client_transmission_malloc(160, 160);
	if (rtc_sess->rtc_channel_t == NULL)
	{
		LOGE("rtc_channel_t malloc failed\n");
	}
	rtos_init_mutex(&rtc_sess->rtc_mutex);
    return rtc_sess;

fail:
	if (rtc_sess) {
		os_free(rtc_sess);
		rtc_sess = NULL;
	}
	return NULL;
}

bk_err_t rtc_websocket_stop(rtc_session *rtc_session)
{
	if(rtc_session->bk_rtc_client) {
		LOGE("%s stop websocket client\r\n", __func__);
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
	if (rtc_session) {
		os_free(rtc_session);
	}

	return BK_OK;
}

int bk_rtc_video_data_send(const uint8_t *data_ptr, size_t data_len, const video_frame_info_t *info_ptr)
{
    int rval = 0;
/*
    agora_rtc_t *rtc = __get_rtc_instance();

    if (!rtc)
    {
        return BK_FAIL;
    }

    // API: send audio data
    video_frame_info_t info = { 0 };
    info.data_type   = info_ptr->data_type;
    info.frame_rate  = info_ptr->frame_rate;
    info.frame_type  = info_ptr->frame_type;
    info.stream_type = info_ptr->stream_type;
    info.rotation    = VIDEO_ORIENTATION_90;

    rval = agora_rtc_send_video_data(rtc->conn_id, data_ptr, data_len, &info);
    if (rval < 0)
    {
        LOGI("send video data failed, rval=%d data_type=%d len=%d frame_type=%d \n",
             rval, info.data_type, (int)data_len, info.frame_type);
    }
    else
    {
        //LOGI( "send video data successfully. len=%d", (int)data_len);
    }
*/
    return rval;
}

bk_err_t bk_rtc_register_video_rx_handle(rtc_video_rx_data_handle video_rx_handle)
{
/*
    agora_rtc_t *rtc = __get_rtc_instance();

    if (!rtc)
    {
        return BK_FAIL;
    }

    rtc->video_rx_data_handle = video_rx_handle;
*/
    return BK_OK;
}

