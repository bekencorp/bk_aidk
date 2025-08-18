#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <components/shell_task.h>
#include <components/event.h>
#include <components/netif_types.h>
#include "bk_rtos_debug.h"
#include "audio_config.h"
#include "agora_rtc.h"
#include "aud_intf.h"
#include "aud_intf_types.h"
#include <driver/media_types.h>
#include <driver/lcd.h>
#include <modules/wifi.h>
#include "modules/wifi_types.h"
#include "media_app.h"
#include "lcd_act.h"
#include "components/bk_uid.h"
#if CONFIG_BK_SMART_CONFIG
#include "bk_smart_config.h"
#endif
// #if CONFIG_AGORA_IOT_SDK
// #include "bk_smart_config_agora_adapter.h"
// #endif
#include "app_event.h"
#include "audio_process.h"
#include "cli.h"

#include "audio_engine.h"
#include "video_engine.h"
#include <driver/aon_rtc.h>

#define TAG "agora_main"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
extern bool rx_spk_data_flag;
#endif//CONFIG_DEBUG_DUMP

#define VIDEO_FRAME_INTERVAL_MS            500


bool g_connected_flag = false;
bool g_agent_offline = true;
static char agora_appid[33] = {0};
static char channel_name[128] = {0};
static char token[256] = CONFIG_AGORA_TOKEN;
static uint32_t uid = CONFIG_AGORA_UID;
static bool audio_en = false;
static bool video_en = false;


static beken_thread_t  agora_thread_hdl = NULL;
static beken_semaphore_t agora_sem = NULL;
bool agora_runing = false;
static agora_rtc_config_t agora_rtc_config = DEFAULT_AGORA_RTC_CONFIG();
static agora_rtc_option_t agora_rtc_option = DEFAULT_AGORA_RTC_OPTION();

static uint32_t g_target_bps = BANDWIDTH_ESTIMATE_MIN_BITRATE;
extern bool smart_config_running;
extern uint32_t volume;
extern uint32_t g_volume_gain[SPK_VOLUME_LEVEL];
extern app_aud_para_t app_aud_cust_para;

#if (CONFIG_IMAGE_DEBUG_DUMP)
static bool video_lock = false;
#endif

extern bk_err_t video_turn_off(void);
extern bk_err_t video_turn_on(void);

static void cli_agora_rtc_help(void)
{
    LOGI("agora_test {start|stop appid video_en channel_name}\n");
    LOGI("agora_debug {dump_mic_data value}\n");
}

extern uint8_t ir_mode_switching;
static void agora_rtc_user_notify_msg_handle(agora_rtc_msg_t *p_msg)
{
    switch (p_msg->code)
    {
        case AGORA_RTC_MSG_JOIN_CHANNEL_SUCCESS:
            g_connected_flag = true;
            LOGI("Join channel success.\n");
            break;
        case AGORA_RTC_MSG_REJOIN_CHANNEL_SUCCESS:
            g_connected_flag = true;
            LOGI("Rejoin channel success.\n");
            if (g_agent_offline == false)
                network_reconnect_stop_timeout_check();
            app_event_send_msg(APP_EVT_RTC_REJOIN_SUCCESS, 0);
            break;
        case AGORA_RTC_MSG_USER_JOINED:
            LOGI("User Joined.\n");
            network_reconnect_stop_timeout_check();
            app_event_send_msg(APP_EVT_AGENT_JOINED, 0);
            g_agent_offline = false;
            smart_config_running = false;
            break;
        case AGORA_RTC_MSG_USER_OFFLINE:
            LOGI("User Offline.\n");
            g_agent_offline = true;
            app_event_send_msg(APP_EVT_AGENT_OFFLINE, 0);
            if (g_connected_flag == true && !ir_mode_switching)
               app_event_send_msg(APP_EVT_AGENT_DEVICE_REMOVE, 0);
            break;
        case AGORA_RTC_MSG_CONNECTION_LOST:
            LOGE("Lost connection. Please check wifi status.\n");
            g_connected_flag = false;
            app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
            break;
        case AGORA_RTC_MSG_INVALID_APP_ID:
            LOGE("Invalid App ID. Please double check.\n");
            break;
        case AGORA_RTC_MSG_INVALID_CHANNEL_NAME:
            LOGE("Invalid channel name. Please double check.\n");
            break;
        case AGORA_RTC_MSG_INVALID_TOKEN:
        case AGORA_RTC_MSG_TOKEN_EXPIRED:
            LOGE("Invalid token. Please double check.\n");
            break;
        case AGORA_RTC_MSG_BWE_TARGET_BITRATE_UPDATE:
            g_target_bps = p_msg->data.bwe.target_bitrate;
            break;
        case AGORA_RTC_MSG_KEY_FRAME_REQUEST:
#if 0
            media_app_h264_regenerate_idr(camera_device.type);
#endif
            break;
        default:
            break;
    }
}


static void memory_free_show(void)
{
    uint32_t total_size, free_size, mini_size;

    LOGI("%-5s   %-5s   %-5s   %-5s   %-5s\r\n", "name", "total", "free", "minimum", "peak");

    total_size = rtos_get_total_heap_size();
    free_size  = rtos_get_free_heap_size();
    mini_size  = rtos_get_minimum_free_heap_size();
    LOGI("heap:\t%d\t%d\t%d\t%d\r\n",  total_size, free_size, mini_size, total_size - mini_size);

#if CONFIG_PSRAM_AS_SYS_MEMORY
    total_size = rtos_get_psram_total_heap_size();
    free_size  = rtos_get_psram_free_heap_size();
    mini_size  = rtos_get_psram_minimum_free_heap_size();
    LOGI("psram:\t%d\t%d\t%d\t%d\r\n", total_size, free_size, mini_size, total_size - mini_size);
#endif
}

static void app_media_read_frame_callback(frame_buffer_t *frame)
{
    video_frame_info_t info = { 0 };
    static uint64_t before = 0, curr = 0;
    curr = bk_aon_rtc_get_ms();

    if (false == g_connected_flag)
    {
        /* agora rtc is not running, do not send video. */
        return;
    }

    info.stream_type = VIDEO_STREAM_HIGH;
    if (frame->fmt == PIXEL_FMT_JPEG)
    {
        info.data_type = VIDEO_DATA_TYPE_GENERIC_JPEG;
        info.frame_type = VIDEO_FRAME_KEY;
    }
    else if (frame->fmt == PIXEL_FMT_H264)
    {
        if ((frame->h264_type & (1 << H264_NAL_I_FRAME)) == 0)
        {
            LOGW("%s, ####not i frame, 0x%8x%08x - 0x%8x%08x : 0x%8%08x###\n", __func__, (uint32_t)(curr>>32), (uint32_t)curr, (uint32_t)(before>>32), (uint32_t)before, (uint32_t)((curr - before)>>32), (uint32_t)(curr - before));
            return;
        }
        info.data_type = VIDEO_DATA_TYPE_H264;
        info.frame_type = VIDEO_FRAME_AUTO_DETECT;
    }
    else if (frame->fmt == PIXEL_FMT_H265)
    {
        info.data_type = VIDEO_DATA_TYPE_H265;
        info.frame_type = VIDEO_FRAME_AUTO_DETECT;
    }
    else
    {
        LOGE("not support format: %d \r\n", frame->fmt);
    }

    if (curr > before && curr - before >= VIDEO_FRAME_INTERVAL_MS)
    {
        LOGI("##########send frame: 0x%x%08x - 0x%x%08x : 0x%x%08x######################\n", (uint32_t)(curr>>32), (uint32_t)curr, (uint32_t)(before>>32), (uint32_t)before, (uint32_t)((curr - before)>>32), (uint32_t)(curr - before));
#if (CONFIG_IMAGE_DEBUG_DUMP)
        do {
#endif
            bk_agora_rtc_video_data_send((uint8_t *)frame->frame, (size_t)frame->length, &info);

            /* send two frame images per second */
#if (CONFIG_IMAGE_DEBUG_DUMP)
        } while (video_lock);
#endif
        before = curr;
    }

}

static int agora_rtc_user_audio_rx_data_handle(unsigned char *data, unsigned int size, const audio_frame_info_t *info_ptr)
{
    bk_err_t ret = BK_OK;

    #if CONFIG_DEBUG_DUMP
    if(rx_spk_data_flag)
    {
        uint32_t decoder_type = bk_aud_get_decoder_type();
        uint8_t dump_file_type = dbg_dump_get_dump_file_type((uint8_t)decoder_type);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DUMP_FILE_TYPE(DUMP_TYPE_RX_SPK,0,dump_file_type);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_RX_SPK,0,size);
        DEBUG_DATA_DUMP_UPDATE_HEADER_TIMESTAMP(DUMP_TYPE_RX_SPK);
        #if CONFIG_DEBUG_DUMP_DATA_TYPE_EXTENSION
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_SAMP_RATE(DUMP_TYPE_RX_SPK,0,bk_aud_get_dac_sample_rate());
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_FRAME_IN_MS(DUMP_TYPE_RX_SPK,0,bk_aud_get_dec_frame_len_in_ms());
        #endif
        DEBUG_DATA_DUMP_BY_UART_HEADER(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_UPDATE_HEADER_SEQ_NUM(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_BY_UART_DATA(data, size);
    }
    #endif//CONFIG_DEBUG_DUMP

    ret = bk_aud_intf_write_spk_data((uint8_t *)data, (uint32_t)size);
    if (ret != BK_OK)
    {
        LOGE("write spk data fail \r\n");
    }

    return ret;
}


void agora_main(void)
{
    bk_err_t ret = BK_OK;
    memory_free_show();

    // 2. API: init agora rtc sdk

    //service_opt.license_value[0] = '\0';
    agora_rtc_config.p_appid = (char *)psram_malloc(strlen(agora_appid) + 1);
    os_strcpy((char *)agora_rtc_config.p_appid, agora_appid);
    agora_rtc_config.log_disable = true;
    agora_rtc_config.bwe_param_max_bps = BANDWIDTH_ESTIMATE_MAX_BITRATE;

    audio_tras_register_tx_data_func(bk_agora_rtc_audio_data_send);
    video_register_tx_data_func(app_media_read_frame_callback);

    ret = bk_agora_rtc_register_audio_rx_handle((agora_rtc_audio_rx_data_handle)agora_rtc_user_audio_rx_data_handle);
    if (ret != BK_OK)
    {
        LOGE("bk_aggora_rtc_register_audio_rx_handle fail, ret:%d \r\n", ret);
    }

    ret = bk_agora_rtc_create(&agora_rtc_config, (agora_rtc_msg_notify_cb)agora_rtc_user_notify_msg_handle);
    if (ret != BK_OK)
    {
        LOGI("bk_agora_rtc_create fail \r\n");
    }
    // LOGI("-----start agora rtc process-----\r\n");

    agora_rtc_option.p_channel_name = (char *)psram_malloc(os_strlen(channel_name) + 1);
    os_strcpy((char *)agora_rtc_option.p_channel_name, channel_name);
    agora_rtc_option.audio_config.audio_data_type = CONFIG_AUDIO_CODEC_TYPE;
#if defined(CONFIG_SEND_PCM_DATA)
    agora_rtc_option.audio_config.pcm_sample_rate = CONFIG_PCM_SAMPLE_RATE;
    agora_rtc_option.audio_config.pcm_channel_num = CONFIG_PCM_CHANNEL_NUM;
#endif
    if (strlen(token))
    {
        LOGI("agora_main use record token:%s \r\n", token);
        agora_rtc_option.p_token = token;
    }
    else
    {
        LOGI("agora_main use default token:%s \r\n", CONFIG_AGORA_TOKEN);
        agora_rtc_option.p_token = CONFIG_AGORA_TOKEN;
    }

    if (uid != CONFIG_AGORA_UID)
    {
        LOGI("agora_main use record uid:%d \r\n", uid);
        agora_rtc_option.uid = uid;
    }
    else
    {
        LOGI("agora_main use default uid:%d \r\n", CONFIG_AGORA_UID);
        agora_rtc_option.uid = CONFIG_AGORA_UID;
    }

    ret = bk_agora_rtc_start(&agora_rtc_option);
    if (ret != BK_OK)
    {
        LOGE("bk_agora_rtc_start fail, ret:%d \r\n", ret);
        return;
    }

    agora_runing = true;

    rtos_set_semaphore(&agora_sem);

    /* wait until we join channel successfully */
    while (!g_connected_flag)
    {
        // memory_free_show();
        //        rtos_dump_task_runtime_stats();
        if (!agora_runing)
        {
            goto exit;
        }
        rtos_delay_milliseconds(100);
    }

    LOGI("-----agora_rtc_join_channel success-----\r\n");

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
    ret = bk_agora_rtc_register_audio_rx_handle((agora_rtc_audio_rx_data_handle)agora_rtc_user_audio_rx_data_handle);
    if (ret != BK_OK)
    {
        LOGE("bk_aggora_rtc_register_audio_rx_handle fail, ret:%d \r\n", ret);
    }
#else
    /* turn on audio */
    if (audio_en)
    {
        ret = audio_turn_on();
        if (ret != BK_OK)
        {
            LOGE("%s, %d, audio turn on fail, ret:%d\n", __func__, __LINE__, ret);
            goto exit;
        }
        memory_free_show();
    }
#endif

    /* turn on video */
    if (video_en)
    {
        ret = video_turn_on();
        if (ret != BK_OK)
        {
            LOGE("%s, %d, video turn on fail, ret:%d\n", __func__, __LINE__, ret);
            goto exit;
        }
        memory_free_show();
    }

    while (agora_runing)
    {
        rtos_delay_milliseconds(100);
        // memory_free_show();
        // rtos_dump_task_runtime_stats();
    }

exit:
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
    /* deregister callback to handle audio data received from agora rtc */
    bk_agora_rtc_register_audio_rx_handle(NULL);
#else
    /* free audio  */
    if (audio_en)
    {
        audio_turn_off();
    }
#endif

    /* free video sources */
    if (video_en)
    {
        video_turn_off();
    }

    /* free agora */
    /* stop agora rtc */
    bk_agora_rtc_stop();

    /* destory agora rtc */
    bk_agora_rtc_destroy();

    if (agora_rtc_config.p_appid)
    {
        psram_free((char *)agora_rtc_config.p_appid);
        agora_rtc_config.p_appid = NULL;
    }

    if (agora_rtc_option.p_channel_name)
    {
        psram_free((char *)agora_rtc_option.p_channel_name);
        agora_rtc_option.p_channel_name = NULL;
    }

    audio_en = false;
    video_en = false;

    g_connected_flag = false;

    /* delete task */
    agora_thread_hdl = NULL;

    agora_runing = false;

    rtos_set_semaphore(&agora_sem);

    rtos_delete_thread(NULL);
}

bk_err_t agora_stop(void)
{
    if (!agora_runing)
    {
        LOGI("agora not start\n");
        return BK_OK;
    }

    agora_runing = false;

    rtos_get_semaphore(&agora_sem, BEKEN_NEVER_TIMEOUT);

    rtos_deinit_semaphore(&agora_sem);
    agora_sem = NULL;

    return BK_OK;
}

static bk_err_t agora_start(void)
{
    bk_err_t ret = BK_OK;

    if (agora_runing)
    {
        LOGI("agora already start, Please close and then reopens\n");
        return BK_FAIL;
    }

    ret = rtos_init_semaphore(&agora_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_create_thread(&agora_thread_hdl,
                             4,
                             "agora",
                             (beken_thread_function_t)agora_main,
                             6 * 1024,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create agora app task fail, ret:%d\n", __func__, __LINE__, ret);
        agora_thread_hdl = NULL;
        goto fail;
    }

    rtos_get_semaphore(&agora_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("create agora app task complete\n");

    return BK_OK;

fail:

    if (agora_sem)
    {
        rtos_deinit_semaphore(&agora_sem);
        agora_sem = NULL;
    }

    return BK_FAIL;
}


void cli_agora_rtc_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc < 2)
    {
        goto cmd_fail;
    }

    /* audio test */
    if (os_strcmp(argv[1], "start") == 0)
    {
        if (argc < 4)
        {
            goto cmd_fail;
        }
        audio_en = true;
        sprintf(agora_appid, "%s", argv[2]);
        if (os_strtoul(argv[3], NULL, 10))
        {
            video_en = true;
        }
        else
        {
            video_en = false;
        }

        if (argc >= 5)
        {
            sprintf(channel_name, "%s", argv[4]);
        }
        else
        {
            unsigned char uid[32] = {0};
            bk_uid_get_data(uid);
            sprintf(channel_name, "%s", uid);
        }

        agora_start();
    }
    else if (os_strcmp(argv[1], "stop") == 0)
    {
        agora_stop();
    }
#if (CONFIG_IMAGE_DEBUG_DUMP)
    else if (os_strcmp(argv[1], "video_lock") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            LOGI("video image lock\n");
            video_lock = true;
        }
        else
        {
            LOGI("video image unlock\n");
            video_lock = false;
        }
    }
#endif
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    cli_agora_rtc_help();
}

#if 0
void cli_beken_aud_dump_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc < 3)
    {
        goto cmd_fail;
    }

    /* audio test */
    if (os_strcmp(argv[1], "dump_mic_data") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            tx_mic_data_flag = true;
        }
        else
        {
            tx_mic_data_flag = false;
        }
    }
#if CONFIG_SYS_CPU1
    /* audio test */
    if (os_strcmp(argv[1], "dump_aec_all_data") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            aec_all_data_flag = true;
        }
        else
        {
            aec_all_data_flag = false;
        }
    }
#endif
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    cli_agora_rtc_help();
}
#endif
/* call this api when wifi autoconnect */
extern char *app_id_record;
extern char *channel_name_record;
extern char *token_record;
extern uint32_t uid_record;
void agora_auto_run(uint8_t reset)
{
#if !CONFIG_BK_DEV_STARTUP_AGENT
    if (!channel_name_record || !app_id_record)
    {
        return;
    }
#endif
    if (bk_sconf_wakeup_agent(reset)) {
        LOGE("%s, wake up agent fail!\n", __func__);
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
        return;
    }
    sprintf(agora_appid, "%s", app_id_record);
    sprintf(channel_name, "%s", channel_name_record);
    if (token_record)
    {
        sprintf(token, "%s", token_record);
    }
    else
    {
        token[0] = '\0';
    }

    if (uid_record)
        uid = uid_record;

    if (!agora_runing)
    {
        audio_en = true;
        video_en = false;
        agora_start();
    }
}

#define AGORA_RTC_CMD_CNT   (sizeof(s_agora_rtc_commands) / sizeof(struct cli_command))

extern void cli_agora_rtc_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);

static const struct cli_command s_agora_rtc_commands[] =
{
    {"agora_test", "agora_test ...", cli_agora_rtc_test_cmd},
};

int agora_rtc_cli_init(void)
{
    return cli_register_commands(s_agora_rtc_commands, AGORA_RTC_CMD_CNT);
}
