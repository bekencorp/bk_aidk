#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#include "volc_rtc.h"
#include "volc_config.h"
#include "volc_fileio.h"
#include "audio_transfer.h"
#include "aud_intf.h"

#define TAG "byte_rtc"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


static byte_rtc_t byte_rtc = {0};

static byte_rtc_t *__get_rtc_instance(void)
{
    return &byte_rtc;
}

static void __send_message_2_user(byte_rtc_t *rtc, byte_rtc_msg_t *msg)
{
    if (rtc->user_message_callback)
    {
        rtc->user_message_callback(msg);
    }
}

static void __rtc_started(byte_rtc_t *rtc)
{
    rtc->state = BYTE_RTC_STATE_WORKING;
}

static bool __is_rtc_started(byte_rtc_t *rtc)
{
    if (rtc->state == BYTE_RTC_STATE_WORKING)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool __get_rtc_status(byte_rtc_t *rtc)
{
    return rtc->state;
}

static void __rtc_stopped(byte_rtc_t *rtc)
{
    rtc->state = BYTE_RTC_STATE_IDLE;
}

static void __deep_copy_items_destroy(byte_rtc_t *rtc);

static void __key_frame_req_msg(byte_rtc_t *rtc)
{
    byte_rtc_msg_t msg;
    msg.code = BYTE_RTC_MSG_KEY_FRAME_REQUEST;
    __send_message_2_user(rtc, &msg);
}

static void __change_encode_bitrate_req_msg(byte_rtc_t *rtc, uint32_t target_bps)
{
    byte_rtc_msg_t msg;
    msg.code =  BYTE_RTC_MSG_BWE_TARGET_BITRATE_UPDATE;
    msg.bwe.target_bitrate = target_bps;
    __send_message_2_user(rtc, &msg);
}

static void __register_message_router(byte_rtc_t *rtc, byte_rtc_msg_notify_cb message_callback)
{
    rtc->user_message_callback = message_callback;
}

static void __on_join_room_success(byte_rtc_engine_t engine, const char *room, int elapsed_ms, bool rejoin)
{
    byte_rtc_msg_t msg;

    LOGI("join room success, engine: %p, room: %s, elapsed_ms: %d, rejoin: %d \n", engine, room, elapsed_ms, rejoin);
    byte_rtc_t *rtc = __get_rtc_instance();

    rtc->b_channel_joined = true;
    if (!rejoin)
        msg.code = BYTE_RTC_MSG_JOIN_CHANNEL_SUCCESS;
    else
        msg.code = BYTE_RTC_MSG_REJOIN_CHANNEL_SUCCESS;

    __send_message_2_user(rtc, &msg);

    __rtc_started(rtc);
}

static void __on_room_error(byte_rtc_engine_t engine, const char* room, int code, const char* msg)
{
    LOGI("join room error, engine: %p, room: %s, code: %d, msg: %s \n", engine, room, code, msg);
}

static void __error_msg(byte_rtc_t *rtc, byte_rtc_msg_e err)
{
    byte_rtc_msg_t msg;
    msg.code = err;
    __send_message_2_user(rtc, &msg);
}

static void __on_error(byte_rtc_engine_t engine, int code, const char *msg)
{
    LOGI("engine: %p, code: %d\nmsg: %s \n", engine, code, msg ? msg : "null");

    byte_rtc_t *rtc = __get_rtc_instance();
    switch (code)
    {
        // case ERR_INVALID_APP_ID:
        //     __error_msg(rtc, BYTE_RTC_MSG_INVALID_APP_ID);
        //     break;
        case ERR_ROOMID_ILLEGAL:
            __error_msg(rtc, BYTE_RTC_MSG_INVALID_CHANNEL_NAME);
            break;
        case ERR_INVALID_TOKEN:
            __error_msg(rtc, BYTE_RTC_MSG_INVALID_TOKEN);
            break;
        case ERR_ROOM_TOKEN_EXPIRED:
            __error_msg(rtc, BYTE_RTC_MSG_TOKEN_EXPIRED);
            break;
        default:
            LOGI("Unknown Error Code: %d \n", code);
            break;
    }
}

static void __on_user_joined(byte_rtc_engine_t engine, const char *room, const char *user_name, int elapsed_ms)
{
    byte_rtc_msg_t msg;

    LOGI("user_joined, engine: %p, room: %s, user: %s, elapsed_ms: %d \n", engine, room, user_name, elapsed_ms);
    byte_rtc_t *rtc = __get_rtc_instance();

    rtc->b_user_joined = true;

    msg.code = BYTE_RTC_MSG_USER_JOINED;
	os_strcpy(msg.user, user_name);
    __send_message_2_user(rtc, &msg);
}

static void __on_user_offline(byte_rtc_engine_t engine, const char *room, const char *user_name, int reason)
{
	byte_rtc_msg_t msg;

    LOGW("engine: %p, room: %s, user: %s, reason: %d \n", engine, room, user_name, reason);
    byte_rtc_t *rtc = __get_rtc_instance();

    rtc->b_user_joined = false;

    msg.code = BYTE_RTC_MSG_USER_OFFLINE;
	os_strcpy(msg.user, user_name);
    __send_message_2_user(rtc, &msg);
}

static void __on_key_frame_gen_req(byte_rtc_engine_t engine, const char * room, const char * user_name)
{
    LOGD("engine: %p, room: %p, user_name: %d \n", engine, room, user_name);
    byte_rtc_t *rtc = __get_rtc_instance();

    __key_frame_req_msg(rtc);
}

static void __on_audio_data(byte_rtc_engine_t engine,const char *room, const char *user_name ,uint16_t sent_ts,
                      audio_data_type_e codec, const void *data_ptr, size_t data_len)
{
    byte_rtc_t *rtc = __get_rtc_instance();
    if (!__is_rtc_started(rtc) || !rtc->b_channel_joined || !rtc->b_user_joined)
    {
        LOGI("Wrong rtc state, rtc_started: %s, room_joined: %s, user_joined: %s \n",
             __is_rtc_started(rtc) ? "true" : "false", rtc->b_channel_joined ? "true" : "false", rtc->b_user_joined ? "true" : "false");
        return;
    }
    // LOGI("room=%s, user=%s sent_ts=%u codec=%u, data_ptr=%p, data_len=%d\r\n",
    //      room, user_name, sent_ts, codec, data_ptr, (int)data_len);
    //LOGI("RTC rx audio, codec=%u, data_ptr=%p, data_len=%d\r\n", codec, data_ptr, (int)data_len);
    if (rtc->audio_rx_data_handle)
    {
        rtc->audio_rx_data_handle((unsigned char *)data_ptr, data_len, codec);
    }
    else
    {
        LOGD("aud_rx_data_handle is NULL \n");
    }
}

static void __on_video_data(byte_rtc_engine_t engine, const char *room, const char *user_name ,uint16_t sent_ts,
                      video_data_type_e codec,  int is_key_frame,
                      const void *data_ptr, size_t data_len)
{
    byte_rtc_t *rtc = __get_rtc_instance();
    if (!__is_rtc_started(rtc) || !rtc->b_channel_joined || !rtc->b_user_joined)
    {
        LOGI("Wrong rtc state, rtc_started: %s, channel_joined: %s, user_joined: %s \n",
             __is_rtc_started(rtc) ? "true" : "false", rtc->b_channel_joined ? "true" : "false", rtc->b_user_joined ? "true" : "false");
        return;
    }
    LOGD("room=%s, user=%p sent_ts=%u codec=%u, data_ptr=%p, data_len=%d",
         room, user_name, sent_ts, codec, data_ptr, (int)data_len);

    if (rtc->video_rx_data_handle)
    {
        rtc->video_rx_data_handle(data_ptr, data_len, codec);
    }
    else
    {
        LOGD("aud_rx_data_handle is NULL \n");
    }
}

static void __on_user_mute_audio(byte_rtc_engine_t engine,const char *room, const char *user_name ,int muted)
{
    LOGI("remote user mute audio  %s:%s %d\r\n",room,user_name,muted);
}

static void __on_user_mute_video(byte_rtc_engine_t engine,const char *room, const char *user_name ,int muted)
{
    LOGI("remote user mute video  %s:%s %d\r\n",room,user_name,muted);
}

static void __on_target_bitrate_changed(byte_rtc_engine_t engine,const char *room, uint32_t target_bps)
{

    byte_rtc_t *rtc = __get_rtc_instance();
    if (!__is_rtc_started(rtc))
    {
        LOGI("pipeline not started. Skip bitrate changed process \n");
        return;
    }
    LOGD("engine: %p, target bitrate will change from %d to %d \n",
         engine, rtc->target_bitrate, target_bps);

    rtc->target_bitrate = target_bps;
    __change_encode_bitrate_req_msg(rtc, target_bps);
}

static void __on_message_received(byte_rtc_engine_t engine,const char * room, const char * src, const uint8_t * message,int size,bool binary)
{
    LOGD("__on_message_received \n");
}
static void __on_message_send_result(byte_rtc_engine_t engine,const char * room,int64_t msgid, int error,const char * extencontent)
{
    LOGD("__on_message_send_result \n");
}
static void __on_token_privilege_will_expire(byte_rtc_engine_t engine,const char * room)
{
    LOGI("__on_token_privilege_will_expire \n");
}
static void __on_license_expire_warning(byte_rtc_engine_t engine,int daysleft)
{
    LOGI("__on_license_expire_warning \n");
}
static void __on_fini_notify(byte_rtc_engine_t engine)
{
    byte_rtc_t *rtc = __get_rtc_instance();

    rtc->fini_notifyed = true;
    LOGI("__on_fini_notify \n");
}

static void __register_byte_rtc_event_handler(byte_rtc_t *rtc)
{
    rtc->byte_rtc_event_handler.on_room_error = __on_room_error;
    rtc->byte_rtc_event_handler.on_join_room_success = __on_join_room_success;
    rtc->byte_rtc_event_handler.on_global_error = __on_error;
    rtc->byte_rtc_event_handler.on_user_joined = __on_user_joined;
    rtc->byte_rtc_event_handler.on_user_offline = __on_user_offline;
    rtc->byte_rtc_event_handler.on_key_frame_gen_req = __on_key_frame_gen_req;
    rtc->byte_rtc_event_handler.on_audio_data = __on_audio_data;
    rtc->byte_rtc_event_handler.on_video_data = __on_video_data;
    rtc->byte_rtc_event_handler.on_target_bitrate_changed = __on_target_bitrate_changed;
    //rtc->byte_rtc_event_handler.on_connection_lost = __on_connection_lost;
    //rtc->byte_rtc_event_handler.on_rejoin_room_success = __on_rejoin_room_success;
    rtc->byte_rtc_event_handler.on_user_mute_audio = __on_user_mute_audio;
    rtc->byte_rtc_event_handler.on_user_mute_video = __on_user_mute_video;
    rtc->byte_rtc_event_handler.on_message_received = __on_message_received;
    rtc->byte_rtc_event_handler.on_message_send_result = __on_message_send_result;
    rtc->byte_rtc_event_handler.on_token_privilege_will_expire = __on_token_privilege_will_expire;
    rtc->byte_rtc_event_handler.on_license_expire_warning = __on_license_expire_warning;
    rtc->byte_rtc_event_handler.on_fini_notify = __on_fini_notify;
}

static void __deep_copy_items_destroy(byte_rtc_t *rtc)
{
    if (rtc->byte_rtc_option.room != NULL)
    {
        psram_free((void *)rtc->byte_rtc_option.room);
        rtc->byte_rtc_option.room = NULL;
    }

}

static int32_t __byte_init(byte_rtc_config_t *p_config)
{
    int ret = 0;
    byte_rtc_t *rtc = __get_rtc_instance();

    rtc->engine = NULL;
    rtc->state = BYTE_RTC_STATE_NULL;
    rtc->fini_notifyed = false;

    rtc->audio_rx_data_handle = NULL;
    rtc->video_rx_data_handle = NULL;
    rtc->user_message_callback = NULL;

    rtc->b_user_joined = false;
    rtc->b_channel_joined = false;

    rtc->target_bitrate = 0;

    rtc->byte_rtc_config.license[0] = '\0';
    rtc->byte_rtc_config.log_level = BYTE_RTC_LOG_LEVEL_INFO;
    rtc->byte_rtc_config.p_appid = (char *)psram_malloc(os_strlen(p_config->p_appid) + 1);
    if (rtc->byte_rtc_config.p_appid == NULL)
    {
        LOGE("malloc app_id fail, size: %d \n", (os_strlen(p_config->p_appid) + 1));
        goto init_error;
    }
    os_strcpy((char *)rtc->byte_rtc_config.p_appid, p_config->p_appid);
    LOGI(" app_id: %s \n", rtc->byte_rtc_config.p_appid);

    os_memcpy(rtc->byte_rtc_config.license, p_config->license, 32);
    LOGI("volc RTC SDK license: %s\n", rtc->byte_rtc_config.license);

    rtc->byte_rtc_config.log_level = p_config->log_level;

    // step1. volc sdk version.
    LOGI("volc RTC SDK v%s \n", byte_rtc_get_version());
    //step4. register event handler
    __register_byte_rtc_event_handler(rtc);

    LOGW("byte_rtc_create appid:%s\n", rtc->byte_rtc_config.p_appid);
    rtc->engine = byte_rtc_create(rtc->byte_rtc_config.p_appid, &rtc->byte_rtc_event_handler);

#if CONFIG_VOLC_RTC_ENABLE_LICENSE
    bool volc_license_valid = false;

    volc_file_exists("/VolcEngineRTCLite.lic", &volc_license_valid);
    if (volc_license_valid)
    {
        ret = byte_rtc_set_params(rtc->engine,"{\"rtc\":{\"root_path\":\"/\"}}");
        if (ret < 0)
        {
            LOGI("byte_rtc_set_params root_path failed, ret=%d error=%s\n", ret, byte_rtc_err_2_str(ret));
            goto init_error;
        }

        ret = byte_rtc_set_params(rtc->engine,"{\"rtc\":{\"license\":{\"enable\":1}}}");
        if (ret < 0)
        {
            LOGI("byte_rtc_set_params enable license failed, ret=%d error=%s\n", ret, byte_rtc_err_2_str(ret));
            goto init_error;
        }
    }
#endif

    byte_rtc_set_log_level(rtc->engine, BYTE_RTC_LOG_LEVEL_WARN);
    if (!rtc->engine)
    {
        LOGW("byte_rtc_create failed\n");
        goto init_error;
    }

    LOGW("byte_rtc_init\n");
    ret = byte_rtc_init(rtc->engine);
    if (ret < 0)
    {
        LOGI("volc rtc init failed, ret=%d error=%s\n", ret, byte_rtc_err_2_str(ret));
        goto init_error;
    }

    return BK_OK;

init_error:
    if (rtc->byte_rtc_config.p_appid != NULL)
    {
        psram_free((void *)rtc->byte_rtc_config.p_appid);
        rtc->byte_rtc_config.p_appid = NULL;
    }

    return BK_FAIL;
}

bk_err_t bk_byte_rtc_create(byte_rtc_config_t *p_config, byte_rtc_msg_notify_cb message_callback)
{
    if (0 != __byte_init(p_config))
    {
        LOGE("__byte_init fail.\n");
        return BK_FAIL;
    }
    else
    {
        LOGI("__byte_init ok.\n");
    }

    byte_rtc_t *rtc = __get_rtc_instance();
    if (rtc)
    {
        __register_message_router(rtc, message_callback);
    }
    else
    {
        return BK_FAIL;
    }
    LOGI("rtc create successfully.\n");
    return BK_OK;
}

bk_err_t bk_byte_rtc_destroy(void)
{
    byte_rtc_t *rtc = __get_rtc_instance();
    uint16_t wait_fini_notify_cnt = 0;
    if (!rtc)
    {
        LOGI("bk_byte_rtc_destroy, rtc invalid\n");
        return BK_FAIL;
    }

    if (!rtc->engine)
    {
        LOGI("bk_byte_rtc_destroy, engine invalid\n");
        return BK_FAIL;
    }

    while ((!rtc->fini_notifyed) && (wait_fini_notify_cnt < BYTE_RTC_WAIT_FINI_TOT_CNT))
    {
        rtos_delay_milliseconds(BYTE_RTC_WAIT_FINI_INTERVAL_MS);
        wait_fini_notify_cnt++;
    }

    LOGI("bk_byte_rtc_destroy, fini_notifyed:%d, wait_cnt:%d\n", rtc->fini_notifyed, wait_fini_notify_cnt);
    byte_rtc_destroy(rtc->engine);

    rtc->state = BYTE_RTC_STATE_IDLE;

    LOGI("rtc destroy successfully.\n");
    return BK_OK;
}

bk_err_t bk_byte_rtc_start(byte_rtc_option_t *option)
{
    int rval = 0;
    byte_rtc_t *rtc = __get_rtc_instance();
    rtc_room_info_t *room;
    if (!rtc)
    {
        return BK_FAIL;
    }

    /*copy room name */
    if (rtc->byte_rtc_option.room)
    {
        psram_free((void *)rtc->byte_rtc_option.room);
        rtc->byte_rtc_option.room = NULL;
    }
    rtc->byte_rtc_option.room = (rtc_room_info_t *)psram_malloc(sizeof(rtc_room_info_t));
    if (rtc->byte_rtc_option.room == NULL)
    {
        LOGE("malloc room fail, size: %d \n", sizeof(rtc_room_info_t));
        goto byte_rtc_start_fail;
    }
    os_memcpy((void *)rtc->byte_rtc_option.room, option->room, sizeof(rtc_room_info_t));
    room = rtc->byte_rtc_option.room;
    LOGI("BYTE RTC SDK room: %s \n", room->room_id);
    //LOGI("BYTE RTC SDK token: %s \n", room->token);

    rtc->byte_rtc_option.audio_data_type = option->audio_data_type;
    os_memcpy((void *)&(rtc->byte_rtc_option.room_options), &(option->room_options), sizeof(byte_rtc_room_options_t));

    // step8. join room
    rval = byte_rtc_set_audio_codec(rtc->engine, rtc->byte_rtc_option.audio_data_type);
    if (0 != rval)
    {
        LOGI("byte_rtc_set_audio_codec failure: %d %s\n", rval, byte_rtc_err_2_str(rval));
        goto byte_rtc_start_fail;
    }

    LOGI("byte_rtc_join_room, room:%s uid:%s subscribe_audio:%d publish_audio:%d subscribe_video:%d publish_video:%d code:%d\n",
         room->room_id, room->uid, rtc->byte_rtc_option.room_options.auto_subscribe_audio, rtc->byte_rtc_option.room_options.auto_publish_audio,
         rtc->byte_rtc_option.room_options.auto_subscribe_video, rtc->byte_rtc_option.room_options.auto_publish_video, rtc->byte_rtc_option.audio_data_type);

    rval = byte_rtc_join_room(rtc->engine, room->room_id, room->uid, room->token, &rtc->byte_rtc_option.room_options);
    if (rval < 0)
    {
        LOGI("join room %s failed, rval=%d error=%s \n",
             room->room_id ? room->room_id : "", rval, byte_rtc_err_2_str(rval));
        goto byte_rtc_start_fail;
    }

    LOGI("Joining room %s ... \n", room->room_id);
    return BK_OK;

byte_rtc_start_fail:
    __deep_copy_items_destroy(rtc);
    return BK_FAIL;
}

bk_err_t bk_byte_rtc_stop(void)
{
    int rval = 0;
    byte_rtc_t *rtc = __get_rtc_instance();

    LOGI("bk_byte_rtc_stop\n");
    if (!rtc)
    {
        LOGI("bk_byte_rtc_stop, rtc invalid\n");
        return BK_FAIL;
    }

    __rtc_stopped(rtc);

    /*  maybe lost connect. but API has covered this case */
    rval = byte_rtc_leave_room(rtc->engine, rtc->byte_rtc_option.room->room_id);
    if (rval < 0)
    {
        LOGI("byte_rtc_leave_room fail, rval=%d error=%s \n", rval, byte_rtc_err_2_str(rval));
        return BK_FAIL;
    }

    rval = byte_rtc_fini(rtc->engine);
    if (rval < 0)
    {
        LOGI("byte_rtc_fini fail, rval=%d error=%s \n", rval, byte_rtc_err_2_str(rval));
        return BK_FAIL;
    }

    if (rtc->byte_rtc_config.p_appid != NULL)
    {
        psram_free((void *)rtc->byte_rtc_config.p_appid);
        rtc->byte_rtc_config.p_appid = NULL;
    }

    if (rtc->byte_rtc_option.room != NULL)
    {
        psram_free((void *)rtc->byte_rtc_option.room);
        rtc->byte_rtc_option.room = NULL;
    }

    return BK_OK;
}

int bk_byte_rtc_video_data_send(const uint8_t *data_ptr, size_t data_len, const video_frame_info_t *info_ptr)
{
    int rval = 0;

    byte_rtc_t *rtc = __get_rtc_instance();

    if (!rtc || (rtc->state != BYTE_RTC_STATE_WORKING))
    {
        return BK_FAIL;
    }

    // API: send audio data
    video_frame_info_t info = { 0 };
    info.data_type   = info_ptr->data_type;
    info.frame_rate  = info_ptr->frame_rate;
    info.frame_type  = info_ptr->frame_type;
    info.stream_type = info_ptr->stream_type;

    rval = byte_rtc_send_video_data(rtc->engine, rtc->byte_rtc_option.room->room_id, data_ptr, data_len, &info);
    if (rval < 0)
    {
        LOGI("send video data failed, rval=%d data_type=%d len=%d frame_type=%d \n",
             rval, info.data_type, (int)data_len, info.frame_type);
    }
    else
    {
        //LOGI( "send video data successfully. len=%d", (int)data_len);
    }

    return rval;
}

uint8_t bk_byte_rtc_audio_codec_type_mapping(uint8_t codec_type)
{
    uint8_t aud_data_type = AUDIO_DATA_TYPE_UNKNOWN;
    switch(codec_type)
    {
        case AUD_INTF_VOC_DATA_TYPE_G711A:
        {
            aud_data_type = AUDIO_DATA_TYPE_PCMA;
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_G711U:
        {
            aud_data_type = AUDIO_DATA_TYPE_PCMU;
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_PCM:
        {
            aud_data_type = AUDIO_DATA_TYPE_PCM;
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_G722:
        {
            aud_data_type = AUDIO_DATA_TYPE_G722;
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_OPUS:
        {
            aud_data_type = AUDIO_DATA_TYPE_OPUS;
            break;
        }
        default:
        {
            LOGE("%s unknown codec type:%d \r\n", __func__,codec_type);
            break;
        }
    }

    return aud_data_type;
}


int bk_byte_rtc_audio_data_send(uint8_t *data_ptr, size_t data_len)
{
    // API: send audio data
    audio_frame_info_t info = { 0 };
    byte_rtc_t *rtc = __get_rtc_instance();

    if (!rtc || (rtc->state != BYTE_RTC_STATE_WORKING))
    {
        return BK_FAIL;
    }
    
    info.data_type = bk_byte_rtc_audio_codec_type_mapping(bk_aud_get_encoder_type());

    int rval = byte_rtc_send_audio_data(rtc->engine, rtc->byte_rtc_option.room->room_id, data_ptr, data_len, &info);
    //LOGI("rtc TX audio, data_ptr=%p, data_len=%d\r\n", data_ptr, (int)data_len);
    if (rval < 0)
    {
        LOGE("Failed to send audio data, reason: %s\n", byte_rtc_err_2_str(rval));
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t bk_byte_rtc_register_audio_rx_handle(byte_rtc_audio_rx_data_handle audio_rx_handle)
{
    byte_rtc_t *rtc = __get_rtc_instance();

    if (!rtc)
    {
        return BK_FAIL;
    }

    rtc->audio_rx_data_handle = audio_rx_handle;

    return BK_OK;
}

bk_err_t bk_byte_rtc_register_video_rx_handle(byte_rtc_video_rx_data_handle video_rx_handle)
{
    byte_rtc_t *rtc = __get_rtc_instance();

    if (!rtc)
    {
        return BK_FAIL;
    }

    rtc->video_rx_data_handle = video_rx_handle;

    return BK_OK;
}
