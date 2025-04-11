/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#ifndef __AGORA_RTC_H__
#define __AGORA_RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

//#include <stdbool.h>

#include "beken_config.h"
#include "bk_websocket_client.h"
#include "cJSON.h"

enum MsgType {
    BEKEN_RTC_SEND_HELLO = 0,
};

/*
*   Magic code  2 bytes
*   Flags       2 bytes
*   Timestamp   4 bytes
*   Squence     2 bytes
*   Length      2 bytes
*   CRC         1 byte
*   RESERVED    3 byte
*/
typedef struct
{
	uint16_t magic;
	uint16_t flags;
	uint32_t timestamp;
	uint16_t sequence;
	uint16_t length;
	uint8_t crc;
	uint8_t reserved[3];
	uint8_t  payload[];
} __attribute__((__packed__)) db_trans_head_t;
typedef int (*rtc_video_rx_data_handle)(const uint8_t *data, size_t size, const video_frame_info_t *info_ptr);
typedef int (*rtc_user_audio_rx_data_handle_cb)(unsigned char *data, unsigned int size, const audio_frame_info_t *info_ptr);

typedef struct
{
	rtc_user_audio_rx_data_handle_cb tsend;
} db_channel_cb_t;

typedef struct
{
	uint8_t *cbuf;
	uint16_t csize;
	uint16_t ccount;
	uint16_t sequence;
	db_trans_head_t *tbuf;
	rtc_user_audio_rx_data_handle_cb cb;
	uint16_t tsize;
} db_channel_t;

/**
 * @brief rx ring buffer
 */
typedef struct {
    uint8_t *buffer;
    size_t head;
    size_t tail;
    beken_semaphore_t mutex;
} data_buffer_t;

/**
 * @brief text info
 */
typedef struct {
    uint8_t text_type;
	char *text_data;
} text_info_t;

typedef struct {
	transport bk_rtc_client;
	db_channel_t *rtc_channel_t;
	beken_mutex_t rtc_mutex;
	data_buffer_t *ab_buffer;
	beken_timer_t data_read_tmr;
}rtc_session;

#define HEAD_SIZE_TOTAL             (sizeof(db_trans_head_t))
#define HEAD_MAGIC_CODE             (0xF0D5)
#define HEAD_FLAGS_CRC              (1 << 0)
#define CRC8_INIT_VALUE 0xFF
rtc_session *rtc_websocket_create(websocket_client_input_t *websocket_cfg, rtc_user_audio_rx_data_handle_cb cb);
bk_err_t rtc_websocket_stop(rtc_session *rtc_session);
int rtc_websocket_audio_send_data(rtc_session *rtc_session, uint8_t *data_ptr, size_t data_len);
void rtc_websocket_audio_receive_data(rtc_session *rtc_session, uint8 *data, uint32_t len);
int rtc_websocket_send_text(transport web_socket, char *str, enum MsgType msgtype);
void rtc_websocket_parse_hello(cJSON *root);
void rtc_websocket_parse_text(text_info_t *text, cJSON *root);
int bk_rtc_video_data_send(const uint8_t *data_ptr, size_t data_len, const video_frame_info_t *info_ptr);
bk_err_t bk_rtc_register_video_rx_handle(rtc_video_rx_data_handle video_rx_handle);
#ifdef __cplusplus
}
#endif
#endif /* __AGORA_RTC_H__ */
