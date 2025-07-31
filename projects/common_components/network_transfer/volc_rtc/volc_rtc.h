#ifndef __BYTE_RTC_H__
#define __BYTE_RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

//#include <stdbool.h>

#include "VolcEngineRTCLite.h"

#define MAX_APPID_LEN       64
#define MAX_ROOMID_LEN      128
#define MAX_USERID_LEN      128
#define MAX_TOKEN_LEN       256
#define MAX_TASKID_LEN      128
#define MAX_BOTUID_LEN      128

#define BYTE_RTC_WAIT_FINI_INTERVAL_MS                  2
#define BYTE_RTC_WAIT_FINI_TOT_CNT                      500

typedef enum
{
    BYTE_RTC_MSG_JOIN_CHANNEL_SUCCESS = 0,
    BYTE_RTC_MSG_REJOIN_CHANNEL_SUCCESS,
    BYTE_RTC_MSG_USER_JOINED,
    BYTE_RTC_MSG_USER_OFFLINE,
    BYTE_RTC_MSG_CONNECTION_LOST,
    BYTE_RTC_MSG_INVALID_APP_ID,
    BYTE_RTC_MSG_INVALID_CHANNEL_NAME,
    BYTE_RTC_MSG_INVALID_TOKEN,
    BYTE_RTC_MSG_TOKEN_EXPIRED,

    BYTE_RTC_MSG_KEY_FRAME_REQUEST,
    BYTE_RTC_MSG_BWE_TARGET_BITRATE_UPDATE,
} byte_rtc_msg_e;

typedef enum
{
    BYTE_RTC_STATE_NULL = 0,
    BYTE_RTC_STATE_IDLE,
    BYTE_RTC_STATE_WORKING,
} byte_rtc_state_t;

typedef struct
{
    uint32_t target_bitrate;
} byte_rtc_bwe_t;

typedef struct
{
    byte_rtc_msg_e code;
    byte_rtc_bwe_t bwe;
    char user[MAX_USERID_LEN+1];
} byte_rtc_msg_t;

typedef void (*byte_rtc_msg_notify_cb)(byte_rtc_msg_t *p_msg);
typedef int (*byte_rtc_audio_rx_data_handle)(unsigned char *data, unsigned int size, audio_data_type_e data_type);
typedef int (*byte_rtc_video_rx_data_handle)(const uint8_t *data, size_t size, video_data_type_e data_type);


typedef struct
{
    char *p_appid;
    char license[33];
    int log_level;
    //area_code_e area_code;
} byte_rtc_config_t;

#define DEFAULT_BYTE_RTC_CONFIG() {                   \
        .p_appid = NULL,                              \
        .license = {0},                               \
        .log_level = BYTE_RTC_LOG_LEVEL_INFO,          \
}

typedef struct
{
    audio_data_type_e audio_data_type;
    int pcm_sample_rate;
    int pcm_channel_num;
} byte_rtc_audio_config_t;

typedef struct {
    char room_id[MAX_ROOMID_LEN+1];
    char uid[MAX_USERID_LEN+1];
    char app_id[MAX_APPID_LEN+1];
    char token[MAX_TOKEN_LEN+1];
    char task_id[MAX_TASKID_LEN+1];
    char bot_uid[MAX_BOTUID_LEN+1];
} rtc_room_info_t;

typedef struct
{
    rtc_room_info_t *room;
    audio_data_type_e audio_data_type;
    byte_rtc_room_options_t room_options;
} byte_rtc_option_t;

#define DEFAULT_BYTE_RTC_OPTION() {                                     \
    .room = NULL,                                                       \
    .audio_data_type = AUDIO_DATA_TYPE_UNKNOWN,                         \
    .room_options = {                                                   \
                        .auto_subscribe_audio = true,                   \
                        .auto_subscribe_video = false,                  \
                        .auto_publish_audio = true,                     \
                        .auto_publish_video = true,                    \
                    },                                                  \
}



typedef struct
{
    byte_rtc_state_t state;
    byte_rtc_engine_t engine;
    byte_rtc_msg_notify_cb user_message_callback;
    bool b_user_joined;
    bool b_channel_joined;
    byte_rtc_event_handler_t byte_rtc_event_handler;

    uint32_t target_bitrate;

    /* receive data handle */
    byte_rtc_audio_rx_data_handle audio_rx_data_handle;
    byte_rtc_video_rx_data_handle video_rx_data_handle;

    byte_rtc_config_t byte_rtc_config;
    byte_rtc_option_t byte_rtc_option;
    bool fini_notifyed;
} byte_rtc_t;


/* API */
bk_err_t bk_byte_rtc_create(byte_rtc_config_t *p_config, byte_rtc_msg_notify_cb message_callback);
bk_err_t bk_byte_rtc_destroy(void);
bk_err_t bk_byte_rtc_start(byte_rtc_option_t *option);
bk_err_t bk_byte_rtc_stop(void);
int bk_byte_rtc_video_data_send(const uint8_t *data_ptr, size_t data_len, const video_frame_info_t *info_ptr);
int bk_byte_rtc_audio_data_send(uint8_t *data_ptr, size_t data_len);
bk_err_t bk_byte_rtc_register_audio_rx_handle(byte_rtc_audio_rx_data_handle audio_rx_handle);
bk_err_t bk_byte_rtc_register_video_rx_handle(byte_rtc_video_rx_data_handle video_rx_handle);
uint8_t bk_byte_rtc_audio_codec_type_mapping(uint8_t codec_type);

#ifdef __cplusplus
}
#endif
#endif /* __BYTE_RTC_H__ */
