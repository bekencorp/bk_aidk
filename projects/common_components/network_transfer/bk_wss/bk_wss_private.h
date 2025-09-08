// Copyright 2025-2035 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __BK_WSS_PRIVATE_H__
#define __BK_WSS_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif

//#include <stdbool.h>

#include "bk_wss_config.h"
#include "bk_websocket_client.h"
#include "cJSON.h"
#include "media_app.h"
#include "audio_engine.h"

/** Error code. */
typedef enum {
  /** No error. */
  ERR_OKAY = 0,

  /** General error */
  ERR_FAILED = 1,

  /** Network is unavailable */
  ERR_NET_DOWN = 14,

  /**
   * Request to join channel is rejected.
   * It occurs when local user is already in channel and try to join the same channel again.
   */
  ERR_JOIN_CHANNEL_REJECTED = 17,

  /** App ID is invalid. */
  ERR_INVALID_APP_ID = 101,

  /** Channel is invalid. */
  ERR_INVALID_CHANNEL_NAME = 102,

  /** Fails to get server resources in the specified region. */
  ERR_NO_SERVER_RESOURCES = 103,

  /** Server rejected request to look up channel. */
  ERR_LOOKUP_CHANNEL_REJECTED = 105,

  /** Server rejected request to open channel. */
  ERR_OPEN_CHANNEL_REJECTED = 107,

  /**
   * Token expired due to reasons belows:
   * - Authorized Timestamp expired:      The timestamp is represented by the number of
   *                                      seconds elapsed since 1/1/1970. The user can use
   *                                      the Token to access the Agora service within five
   *                                      minutes after the Token is generated. If the user
   *                                      does not access the Agora service after five minutes,
   *                                      this Token will no longer be valid.
   * - Call Expiration Timestamp expired: The timestamp indicates the exact time when a
   *                                      user can no longer use the Agora service (for example,
   *                                      when a user is forced to leave an ongoing call).
   *                                      When the value is set for the Call Expiration Timestamp,
   *                                      it does not mean that the Token will be expired,
   *                                      but that the user will be kicked out of the channel.
   */
  ERR_TOKEN_EXPIRED = 109,

  /**
  * Token is invalid due to reasons belows:
  * - If application certificate is enabled on the Dashboard,
  *   valid token SHOULD be set when invoke.
  *
  * - If uid field is mandatory, users must specify the same uid as the one used to generate the token,
  *   when calling `agora_rtc_join_channel`.
  */
  ERR_INVALID_TOKEN = 110,

  /** Dynamic token has been enabled, but is not provided when joining the channel.
   *  Please specify the valid token when calling `agora_rtc_join_channel`.
   */
  ERR_DYNAMIC_TOKEN_BUT_USE_STATIC_KEY = 115,

  /** Switching roles failed.
   *  Please try to rejoin the channel.
   */
  ERR_SET_CLIENT_ROLE_NOT_AUTHORIZED = 119,

  /** Decryption fails. The user may have used a different encryption password to join the channel.
   *  Check your settings or try rejoining the channel.
   */
  ERR_DECRYPTION_FAILED = 120,

  /* Ticket to open channel is invalid */
  ERR_OPEN_CHANNEL_INVALID_TICKET = 121,

  /* Try another server. */
  ERR_OPEN_CHANNEL_TRY_NEXT_VOS = 122,

  /** Client is banned by the server */
  ERR_CLIENT_IS_BANNED_BY_SERVER = 123,

  /** Sending video data too fast and over the bandwidth limit.
   *  Very likely that packet loss occurs with this sending speed.
   */
  ERR_SEND_VIDEO_OVER_BANDWIDTH_LIMIT = 200,

  /** Audio decoder does not match incoming audio data type.
   *  Currently SDK built-in audio codec only supports G722 and OPUS.
   */
  ERR_AUDIO_DECODER_NOT_MATCH_AUDIO_FRAME = 201,

  /** Audio decoder does not match incoming audio data type.
   *  Currently SDK built-in audio codec only supports G722 and OPUS.
   */
  ERR_NO_AUDIO_DECODER_TO_HANDLE_AUDIO_FRAME = 202,
} agora_err_code_e;

/**
 * The definition of the user_offline_reason_e enum.
 */
typedef enum {
  /**
   * 0: Remote user leaves channel actively
   */
  USER_OFFLINE_QUIT = 0,
  /**
   * 1: Remote user is dropped due to timeout
   */
  USER_OFFLINE_DROPPED = 1,
} user_offline_reason_e;

/**
 * The definition of the video_data_type_e enum.
 */
typedef enum {
  /* 0: YUV420 */
  VIDEO_DATA_TYPE_YUV420 = 0,
  /* 2: H264 */
  VIDEO_DATA_TYPE_H264 = 2,
  /* 3: H265 */
  VIDEO_DATA_TYPE_H265 = 3,
  /* 6: generic */
  VIDEO_DATA_TYPE_GENERIC = 6,
  /* 20: generic JPEG */
  VIDEO_DATA_TYPE_GENERIC_JPEG = 20,
} video_data_type_e;

/**
 * The definition of the video_frame_type_e enum.
 */
typedef enum {
  /**
   * 0: unknow frame type
   * If you set it ,the SDK will judge the frame type
   */
  VIDEO_FRAME_AUTO_DETECT = 0,
  /**
   * 3: key frame
   */
  VIDEO_FRAME_KEY = 3,
  /*
   * 4: delta frame, e.g: P-Frame
   */
  VIDEO_FRAME_DELTA = 4,
} video_frame_type_e;

/**
 * The definition of the video_frame_rate_e enum.
 */
typedef enum {
  /** 1: 1 fps. */
  VIDEO_FRAME_RATE_FPS_1 = 1,
  /** 7: 7 fps. */
  VIDEO_FRAME_RATE_FPS_7 = 7,
  /** 10: 10 fps. */
  VIDEO_FRAME_RATE_FPS_10 = 10,
  /** 15: 15 fps. */
  VIDEO_FRAME_RATE_FPS_15 = 15,
  /** 24: 24 fps. */
  VIDEO_FRAME_RATE_FPS_24 = 24,
  /** 30: 30 fps. */
  VIDEO_FRAME_RATE_FPS_30 = 30,
  /** 60: 60 fps. Applies to Windows and macOS only. */
  VIDEO_FRAME_RATE_FPS_60 = 60,
} video_frame_rate_e;

/**
 * Video stream types.
 */
typedef enum {
  /**
   * 0: The high-quality video stream, which has a higher resolution and bitrate.
   */
  VIDEO_STREAM_HIGH = 0,
  /**
   * 1: The low-quality video stream, which has a lower resolution and bitrate.
   */
  VIDEO_STREAM_LOW = 1,
} video_stream_type_e;

/**
 * The definition of the video_frame_info_t struct.
 */
typedef struct {
  /**
   * The video data type: #video_data_type_e.
   */
  video_data_type_e data_type;
  /**
   * The video stream type: #video_stream_type_e
   */
  video_stream_type_e stream_type;
  /**
   * The frame type of the encoded video frame: #video_frame_type_e.
   */
  video_frame_type_e frame_type;
  /**
   * The number of video frames per second.
   * -This value will be used for calculating timestamps of the encoded image.
   * - If frame_per_sec equals zero, then real timestamp will be used.
   * - Otherwise, timestamp will be adjusted to the value of frame_per_sec set.
   */
  video_frame_rate_e frame_rate;
} video_frame_info_t;

/**
 * Audio codec type list.
 */
typedef enum {
  /**
   * 0: Disable
   */
  AUDIO_CODEC_DISABLED = 0,
  /**
   * 1: OPUS
   */
  AUDIO_CODEC_TYPE_OPUS = 1,
  /**
   * 2: G722
   */
  AUDIO_CODEC_TYPE_G722 = 2,
  /**
   * 3: G711A
   */
  AUDIO_CODEC_TYPE_G711A = 3,
  /**
   * 4: G711U
   */
  AUDIO_CODEC_TYPE_G711U = 4,
} audio_codec_type_e;

/**
 * Audio data type list.
 */
typedef enum {
  /**
   * 1: OPUS
   */
  AUDIO_DATA_TYPE_OPUS = 1,
  /**
   * 2: OPUSFB
   */
  AUDIO_DATA_TYPE_OPUSFB = 2,
  /**
   * 3: PCMA
   */
  AUDIO_DATA_TYPE_PCMA = 3,
  /**
   * 4: PCMU
   */
  AUDIO_DATA_TYPE_PCMU = 4,
  /**
   * 5: G722
   */
  AUDIO_DATA_TYPE_G722 = 5,
  /**
   * 8: AACLC
   */
  AUDIO_DATA_TYPE_AACLC = 8,
  /**
   * 9: HEAAC
   */
  AUDIO_DATA_TYPE_HEAAC = 9,
  /**
   * 100: PCM (audio codec should be enabled)
   */
  AUDIO_DATA_TYPE_PCM = 100,
  /**
   * 253: GENERIC
   */
  AUDIO_DATA_TYPE_GENERIC = 253,
} audio_data_type_e;

/**
 * The definition of the audio_frame_info_t struct.
 */
typedef struct {
  /**
   * Audio data type, reference #audio_data_type_e.
   */
  audio_data_type_e data_type;
} audio_frame_info_t;

/**
 * The definition of log level enum
 */
typedef enum {
  RTC_LOG_DEFAULT = 0, // the same as RTC_LOG_NOTICE
  RTC_LOG_EMERG, // system is unusable
  RTC_LOG_ALERT, // action must be taken immediately
  RTC_LOG_CRIT, // critical conditions
  RTC_LOG_ERROR, // error conditions
  RTC_LOG_WARNING, // warning conditions
  RTC_LOG_NOTICE, // normal but significant condition, default level
  RTC_LOG_INFO, // informational
  RTC_LOG_DEBUG, // debug-level messages
} rtc_log_level_e;

/**
 * IP areas.
 */
typedef enum {
  /* the same as AREA_CODE_GLOB */
  AREA_CODE_DEFAULT = 0x00000000,
  /* Mainland China. */
  AREA_CODE_CN = 0x00000001,
  /* North America. */
  AREA_CODE_NA = 0x00000002,
  /* Europe. */
  AREA_CODE_EU = 0x00000004,
  /* Asia, excluding Mainland China. */
  AREA_CODE_AS = 0x00000008,
  /* Japan. */
  AREA_CODE_JP = 0x00000010,
  /* India. */
  AREA_CODE_IN = 0x00000020,
  /* Oceania */
  AREA_CODE_OC = 0x00000040,
  /* South-American */
  AREA_CODE_SA = 0x00000080,
  /* Africa */
  AREA_CODE_AF = 0x00000100,
  /* South Korea */
  AREA_CODE_KR = 0x00000200,
  /* The global area (except China) */
  AREA_CODE_OVS = 0xFFFFFFFE,
  /* (Default) Global. */
  AREA_CODE_GLOB = (0xFFFFFFFF),
} area_code_e;



enum MsgType {
    BEKEN_RTC_SEND_HELLO                    = 0,
    BEKEN_RTC_SESSION_UPDATE                = 1,
    BEKEN_RTC_SESSION_UPDATED               = 2,
    BEKEN_RTC_INPUT_AUDIO_BUFFER_APPEND     = 3,
    BEKEN_RTC_INPUT_AUDIO_BUFFER_CLEAR      = 4,
    BEKEN_RTC_INPUT_AUDIO_BUFFER_COMMIT     = 5,
    BEKEN_RTC_INPUT_AUDIO_BUFFER_COMMITTED  = 6,
    BEKEN_RTC_RESPONSE_CREATE               = 7,
    BEKEN_RTC_RESPONSE_CREATED              = 8,
    BEKEN_RTC_RESPONSE_AUDIO_DONE           = 9,
    BEKEN_RTC_ERROR                         = 10,
    BEKEN_RTC_SEND_FC_FLAG                  = 11,
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
	uint16_t last_seq;
	db_trans_head_t *tbuf;
	rtc_user_audio_rx_data_handle_cb cb;
	uint16_t tsize;
} db_channel_t;

/**
 * @brief rx ring buffer, fixed length
 */
typedef struct {
    uint8_t *buffer;
    size_t head;
    size_t tail;
    size_t size;
} data_buffer_fixed_t;

/**
 * @brief rx ring buffer
 */
typedef struct {
	uint8_t *buffer;
	size_t size;
	size_t read_index;
	size_t write_index;
	size_t *length_buffer;
	size_t length_read_index;
	size_t length_write_index;
	size_t buffer_count;
} data_buffer_t;

/**
 * @brief text info
 */
typedef struct {
    uint8_t text_type;
    char *text_data;
    const char *devId;
    const char *nfcId;
    const char *input_audio_format;
    int input_audio_rate;
    const char *output_audio_format;
    int output_audio_rate;
    const char *user_text;
    const char *mode;
    bool playback_complete;
    bool multi_turn_mode;
} text_info_t;

typedef struct
{
    beken_thread_t thread;
    beken_queue_t queue;
} wss_evt_info_t;

typedef struct {
	transport bk_rtc_client;
	db_channel_t *rtc_channel_t;
	beken_mutex_t rtc_mutex;
	data_buffer_t *ring_buffer;
	data_buffer_fixed_t *ab_buffer;
	beken_timer_t data_read_tmr;
	audio_info_t audio_info;
	int disconnecting_state;
	uint16_t playing_state;
	uint16_t recording_state;
	wss_evt_info_t wss_evt_info;
	uint16_t fc_is_acked;
	// For 4-packet combination sending of PCM audio data
	uint8_t *send_cache_buffer;
	size_t send_cache_size;
	int send_cache_count;
}rtc_session;

typedef struct {
    char devId[32];
    char nfcId[32];
    char input_audio_format[16];
    uint32_t input_audio_rate;
    char output_audio_format[16];
    uint32_t output_audio_rate;
    uint32_t cloud_vad;
    char source[16];
}dialog_session_t;

typedef struct error_info {
    char *error_desc;
} error_info_t;

typedef enum
{
    WSS_EVT_SERVER_HELLO           = 0,
    WSS_EVT_SERVER_SESSION_UPDATED,
    WSS_EVT_SERVER_BUF_COMMITED,
    WSS_EVT_SERVER_RSP_CREATED,
    WSS_EVT_SERVER_RSP_AUDIO_DONE,

    WSS_EVT_HELLO,
    WSS_EVT_SESSION_UPDATE,
    WSS_EVT_AUDIO_BUF_APPEND,
    WSS_EVT_AUDIO_BUF_CLEAR,
    WSS_EVT_AUDIO_BUF_COMMIT,
    WSS_EVT_RSP_CREATE,
    WSS_EVT_SEND_FC_FLAG,
    WSS_EVT_SUBTITLE_DISPLAY,
    WSS_EVT_EXIT,
} wss_evt_type_t;

typedef struct
{
    uint32_t event;
    uint32_t param;
} wss_evt_msg_t;

#define HEAD_SIZE_TOTAL             (sizeof(db_trans_head_t))
#define HEAD_MAGIC_CODE             (0xF0D5)
#define HEAD_FLAGS_CRC              (1 << 0)
#define CRC8_INIT_VALUE 0xFF
rtc_session *__get_beken_rtc(void);
extern dialog_session_t dialog_info;
#if CONFIG_SINGLE_SCREEN_FONT_DISPLAY
extern uint8_t lvgl_app_init_flag;
#endif

rtc_session *rtc_websocket_create(websocket_client_input_t *websocket_cfg, rtc_user_audio_rx_data_handle_cb cb, audio_info_t *info);
bk_err_t rtc_websocket_stop(rtc_session *rtc_session);
int rtc_websocket_audio_send_data(uint8_t *data_ptr, size_t data_len);
void rtc_websocket_audio_receive_data(rtc_session *rtc_session, uint8 *data, uint32_t len);
void rtc_websocket_audio_receive_text(rtc_session *rtc_session, uint8 *data, uint32_t len);
void rtc_websocket_audio_receive_data_general(rtc_session *rtc_session, uint8 *data, uint32_t len);
int rtc_websocket_send_text(rtc_session *rtc_session, void *str, enum MsgType msgtype);
int rtc_websocket_parse_hello(cJSON *root);
void rtc_websocket_parse_text(text_info_t *text, cJSON *root);
void rtc_websocket_parse_request_text(text_info_t *info, cJSON *root);
void rtc_fill_audio_info(audio_info_t *info, char *enctype, char *dectype, uint32_t adc_rate, uint32_t dac_rate, uint32_t enc_ms, uint32_t dec_ms, uint32_t enc_size, uint32_t dec_size);
int bk_rtc_video_data_send(const uint8_t *data_ptr, size_t data_len, const video_frame_info_t *info_ptr);
bk_err_t bk_rtc_register_video_rx_handle(rtc_video_rx_data_handle video_rx_handle);
void parse_session_updated(text_info_t *info, cJSON *root);
void parse_audio_committed(text_info_t *info, cJSON *root);
void parse_text_response(text_info_t *info, cJSON *root);
void parse_audio_frame(text_info_t *info, const uint8_t *data, size_t len);
void parse_audio_done(text_info_t *info, cJSON *root);
void rtc_fill_dialog_info(dialog_session_t *dialog_info, char *devId, char *nfcId,
                                char *input_audio_format, uint32_t input_audio_rate,
                                char *output_audio_format, uint32_t output_audio_rate, uint32_t cloud_vad, char *source);
bk_err_t websocket_event_send_msg(uint32_t event, uint32_t param);
int wss_record_work(rtc_session *handle);
void wss_record_stop(rtc_session *handle);
void wss_record_start(rtc_session *handle);
void wss_play_start(rtc_session *handle);
void wss_play_stop(rtc_session *handle);

#ifdef __cplusplus
}
#endif
#endif /* __AGORA_RTC_H__ */
