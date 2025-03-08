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
#include "agora_config.h"
#include "agora_rtc.h"
#include "audio_transfer.h"
#include <driver/media_types.h>
#include <driver/lcd.h>
#include <modules/wifi.h>
#include "modules/wifi_types.h"
#include "media_app.h"
#include "lcd_act.h"

#include "agora_rtc_demo.h"
#include "media_evt.h"
#include "media_mailbox_list_util.h"
#include "components/bk_uid.h"


#define TAG "agora_main"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#define AUDIO_MIC_TX_RB_SIZE        (16000)//1s data, type: PCM
#define AUDIO_SPK_RX_RB_SIZE        (16000)//1s data, type: PCM

static bool g_connected_flag = false;
static char agora_appid[50] = {0};
static char channel_name[50] = {0};
static bool audio_en = false;
static bool video_en = false;
static media_camera_device_t camera_device =
{
#if defined(CONFIG_UVC_CAMERA)
    .type = UVC_CAMERA,
    .mode = JPEG_MODE,
    .fmt  = PIXEL_FMT_JPEG,
    /* expect the width and length */
    .info.resolution.width  = 640,//640,//864,
    .info.resolution.height = 480,
    .info.fps = FPS25,
#elif defined(CONFIG_DVP_CAMERA)
    /* DVP Camera */
    .type = DVP_CAMERA,
    .mode = H264_MODE,//JPEG_MODE
    .fmt  = PIXEL_FMT_H264,//PIXEL_FMT_JPEG
    /* expect the width and length */
    .info.resolution.width  = 1280,//1280,
    .info.resolution.height = 720,//720,
    .info.fps = FPS30,
#endif
};


static beken_thread_t  agora_thread_hdl = NULL;
static beken_semaphore_t agora_sem = NULL;
static bool agora_runing = false;
static agora_rtc_config_t agora_rtc_config = DEFAULT_AGORA_RTC_CONFIG();
static agora_rtc_option_t agora_rtc_option = DEFAULT_AGORA_RTC_OPTION();

static uint32_t g_target_bps = BANDWIDTH_ESTIMATE_MIN_BITRATE;
static audio_rb_ctx_t audio_rb_ctx = {0};
static bool audio_running_flag = false;
bool agoora_tx_mic_data_flag = false;


#if CONFIG_WIFI_ENABLE
extern void rwnxl_set_video_transfer_flag(uint32_t video_transfer_flag);
#else
#define rwnxl_set_video_transfer_flag(...)
#endif


static void cli_agora_rtc_help(void)
{
    LOGI("agora_test {start|stop appid video_en channel_name}\n");
    LOGI("agora_debug {dump_mic_data value}\n");
}

static void memory_free_show(void)
{
    uint32_t total_size, free_size, mini_size;

    LOGI("%-5s   %-5s   %-5s   %-5s   %-5s\n", "name", "total", "free", "minimum", "peak");

    total_size = rtos_get_total_heap_size();
    free_size  = rtos_get_free_heap_size();
    mini_size  = rtos_get_minimum_free_heap_size();
    LOGI("heap:\t%d\t%d\t%d\t%d\n",  total_size, free_size, mini_size, total_size - mini_size);

#if CONFIG_PSRAM_AS_SYS_MEMORY
    total_size = rtos_get_psram_total_heap_size();
    free_size  = rtos_get_psram_free_heap_size();
    mini_size  = rtos_get_psram_minimum_free_heap_size();
    LOGI("psram:\t%d\t%d\t%d\t%d\n", total_size, free_size, mini_size, total_size - mini_size);
#endif
}

static void agora_rtc_user_notify_msg_handle(agora_rtc_msg_t *p_msg)
{
    switch (p_msg->code)
    {
        case AGORA_RTC_MSG_JOIN_CHANNEL_SUCCESS:
            g_connected_flag = true;
            LOGI("Join channel success.\n");
            break;
        case AGORA_RTC_MSG_USER_JOINED:
            LOGI("User Joined.\n");
            break;
        case AGORA_RTC_MSG_USER_OFFLINE:
            LOGI("User Offline.\n");
            break;
        case AGORA_RTC_MSG_CONNECTION_LOST:
            LOGE("Lost connection. Please check wifi status.\n");
            g_connected_flag = false;
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


#if defined(CONFIG_UVC_CAMERA)
static void media_checkout_uvc_device_info(bk_uvc_device_brief_info_t *info, uvc_state_t state)
{
    bk_uvc_config_t uvc_config_info_param = {0};
    uint8_t format_index = 0;
    uint8_t frame_num = 0;
    uint8_t index = 0;

    if (state == UVC_CONNECTED)
    {
        uvc_config_info_param.vendor_id  = info->vendor_id;
        uvc_config_info_param.product_id = info->product_id;

        format_index = info->format_index.mjpeg_format_index;
        frame_num    = info->all_frame.mjpeg_frame_num;
        if (format_index > 0)
        {
            LOGI("%s uvc_get_param MJPEG format_index:%d\r\n", __func__, format_index);
            for (index = 0; index < frame_num; index++)
            {
                LOGI("uvc_get_param MJPEG width:%d heigth:%d index:%d\r\n",
                     info->all_frame.mjpeg_frame[index].width,
                     info->all_frame.mjpeg_frame[index].height,
                     info->all_frame.mjpeg_frame[index].index);
                for (int i = 0; i < info->all_frame.mjpeg_frame[index].fps_num; i++)
                {
                    LOGI("uvc_get_param MJPEG fps:%d\r\n", info->all_frame.mjpeg_frame[index].fps[i]);
                }

                if (info->all_frame.mjpeg_frame[index].width == camera_device.info.resolution.width
                    && info->all_frame.mjpeg_frame[index].height == camera_device.info.resolution.height)
                {
                    uvc_config_info_param.frame_index = info->all_frame.mjpeg_frame[index].index;
                    uvc_config_info_param.fps         = info->all_frame.mjpeg_frame[index].fps[0];
                    uvc_config_info_param.width       = camera_device.info.resolution.width;
                    uvc_config_info_param.height      = camera_device.info.resolution.height;
                }
            }
        }

        uvc_config_info_param.format_index = format_index;

        if (media_app_set_uvc_device_param(&uvc_config_info_param) != BK_OK)
        {
            LOGE("%s, failed\n, __func__");
        }
    }
    else
    {
        LOGI("%s, %d\n", __func__, state);
    }
}
#endif

void app_media_read_frame_callback(frame_buffer_t *frame)
{
    video_frame_info_t info = { 0 };

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

    bk_agora_rtc_video_data_send((uint8_t *)frame->frame, (size_t)frame->length, &info);

    /* send two frame images per second */
    rtos_delay_milliseconds(500);
}

static int agora_rtc_user_audio_rx_data_handle(unsigned char *data, unsigned int size, const audio_frame_info_t *info_ptr)
{
    uint32_t write_size = 0;

    uint32_t free_size = ring_buffer_get_free_size(&audio_rb_ctx.aud_rx_rb);
    if (free_size >= size)
    {
        write_size = ring_buffer_write(&audio_rb_ctx.aud_rx_rb, (uint8_t *)data, size);
        if (write_size != size)
        {
            LOGE("ring_buffer_write: %d != %d\n", write_size, size);
        }
    }

    return BK_OK;
}

static bk_err_t video_turn_off(void)
{
    bk_err_t ret =  BK_OK;
    LOGI("%s\n", __func__);

    ret = media_app_unregister_read_frame_callback();
    if (ret != BK_OK)
    {
        LOGE("%s, %d, unregister read_frame_cb failed\n", __func__, __LINE__);
    }

    ret = media_app_h264_pipeline_close();
    if (ret != BK_OK)
    {
        LOGE("%s, %d, h264_pipeline_close failed\n", __func__, __LINE__);
    }

    ret = media_app_camera_close(camera_device.type);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, media_app_camera_close failed\n", __func__, __LINE__);
    }

    rwnxl_set_video_transfer_flag(false);

    bk_wifi_set_wifi_media_mode(false);
    bk_wifi_set_video_quality(WIFI_VIDEO_QUALITY_HD);

    return BK_OK;
}


static bk_err_t video_turn_on(void)
{
    bk_err_t ret = BK_OK;
    LOGI("%s\n", __func__);

    bk_wifi_set_wifi_media_mode(true);
    bk_wifi_set_video_quality(WIFI_VIDEO_QUALITY_FD);

    rwnxl_set_video_transfer_flag(true);

#if defined(CONFIG_UVC_CAMERA)
    media_app_uvc_register_info_notify_cb(media_checkout_uvc_device_info);
#endif

    ret = media_app_camera_open(&camera_device);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, media_app_camera_open failed\n", __func__, __LINE__, ret);
        goto fail;
    }

    bool media_mode = false;
    uint8_t quality = 0;
    bk_wifi_get_wifi_media_mode_config(&media_mode);
    bk_wifi_get_video_quality_config(&quality);
    LOGE("~~~~~~~~~~wifi media mode %d, video quality %d~~~~~~\r\n", media_mode, quality);

#if defined(CONFIG_UVC_CAMERA)
    ret = media_app_h264_pipeline_open();
    if (ret != BK_OK)
    {
        LOGE("%s, %d, h264_pipeline_open failed, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }

    ret = media_app_register_read_frame_callback(PIXEL_FMT_H264, app_media_read_frame_callback);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, register read_frame_cb failed\n", __func__, __LINE__, ret);
        goto fail;
    }
#elif defined(CONFIG_DVP_CAMERA)
    ret = media_app_register_read_frame_callback(camera_device.fmt, app_media_read_frame_callback);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, register read_frame_cb failed\n", __func__, __LINE__, ret);
        goto fail;
    }
#endif
    memory_free_show();

    return BK_OK;

fail:
    video_turn_off();

    return BK_FAIL;
}

static bk_err_t audio_turn_off(void)
{
    bk_err_t ret =  BK_OK;
    LOGI("%s\n", __func__);

    if (!audio_running_flag)
    {
        LOGI("audio already turn off\n");
        return BK_OK;
    }

    /* deregister callback to handle audio data received from agora rtc */
    bk_agora_rtc_register_audio_rx_handle(NULL);

    /* start audio task in cpu1 */
    ret = msg_send_req_to_media_app_mailbox_sync(EVENT_AUD_AI_STOP_REQ, 0, NULL);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, stop cpu1 audio fail, ret:%d\n", __func__, __LINE__, ret);
    }

    /* deinit audio task in cpu1 */
    ret = msg_send_req_to_media_app_mailbox_sync(EVENT_AUD_AI_DEINIT_REQ, 0, NULL);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, deinit cpu1 audio fail, ret:%d\n", __func__, __LINE__, ret);
    }

    /* deinit audio transfer */
    audio_tras_deinit();

    /* free tx ringbuffer and rx ringbuffer */
    if (audio_rb_ctx.tx_ring_buff)
    {
        ring_buffer_clear(&audio_rb_ctx.aud_tx_rb);
        psram_free(audio_rb_ctx.tx_ring_buff);
        audio_rb_ctx.tx_ring_buff = NULL;
    }

    if (audio_rb_ctx.rx_ring_buff)
    {
        ring_buffer_clear(&audio_rb_ctx.aud_rx_rb);
        psram_free(audio_rb_ctx.rx_ring_buff);
        audio_rb_ctx.rx_ring_buff = NULL;
    }

    audio_running_flag = false;

    return BK_OK;
}

static bk_err_t audio_turn_on(void)
{
    bk_err_t ret = BK_OK;
    LOGI("%s\n", __func__);

    if (audio_running_flag)
    {
        LOGI("audio already turn on\n");
        return BK_OK;
    }

    /* create task to transfer mic data to AI Agent */
    ret = audio_tras_init(&audio_rb_ctx.aud_tx_rb);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, audio transfer init fail, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }

    /* tx config */
    audio_rb_ctx.tx_size = AUDIO_MIC_TX_RB_SIZE;
    audio_rb_ctx.tx_ring_buff = psram_malloc(audio_rb_ctx.tx_size);
    if (audio_rb_ctx.tx_ring_buff == NULL)
    {
        LOGE("%s, %d, malloc tx buffer fail\n", __func__, __LINE__);
        goto fail;
    }
    ring_buffer_init(&audio_rb_ctx.aud_tx_rb, (uint8_t *)audio_rb_ctx.tx_ring_buff, audio_rb_ctx.tx_size, DMA_ID_MAX, RB_DMA_TYPE_NULL);

    /* rx config */
    audio_rb_ctx.rx_size = AUDIO_SPK_RX_RB_SIZE;
    audio_rb_ctx.rx_ring_buff = psram_malloc(audio_rb_ctx.rx_size);
    if (audio_rb_ctx.rx_ring_buff == NULL)
    {
        LOGE("%s, %d, malloc rx buffer fail\n", __func__, __LINE__);
        goto fail;
    }
    ring_buffer_init(&audio_rb_ctx.aud_rx_rb, (uint8_t *)audio_rb_ctx.rx_ring_buff, audio_rb_ctx.rx_size, DMA_ID_MAX, RB_DMA_TYPE_NULL);

    /* init audio task in cpu1 */
    ret = msg_send_req_to_media_app_mailbox_sync(EVENT_AUD_AI_INIT_REQ, (uint32_t)&audio_rb_ctx, NULL);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, init cpu1 audio fail, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }

    /* register callback to handle audio data received from agora rtc */
    ret = bk_agora_rtc_register_audio_rx_handle((agora_rtc_audio_rx_data_handle)agora_rtc_user_audio_rx_data_handle);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, bk_agora_rtc_register_audio_rx_handle fail, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }

    /* start audio task in cpu1 */
    ret = msg_send_req_to_media_app_mailbox_sync(EVENT_AUD_AI_START_REQ, 0, NULL);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, start cpu1 audio fail, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }

    audio_running_flag = true;

    return BK_OK;

fail:
    audio_turn_off();

    return BK_FAIL;
}

void agora_main(void)
{
    bk_err_t ret = BK_OK;

    memory_free_show();

    /* init agora rtc sdk */
    agora_rtc_config.p_appid = (char *)psram_malloc(strlen(agora_appid) + 1);
    os_strcpy((char *)agora_rtc_config.p_appid, agora_appid);
    agora_rtc_config.log_disable = true;
    agora_rtc_config.bwe_param_max_bps = BANDWIDTH_ESTIMATE_MAX_BITRATE;
    ret = bk_agora_rtc_create(&agora_rtc_config, (agora_rtc_msg_notify_cb)agora_rtc_user_notify_msg_handle);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, bk_agora_rtc_create fail, ret:%d\n", __func__, __LINE__, ret);
        goto exit;
    }

    agora_rtc_option.p_channel_name = (char *)psram_malloc(os_strlen(channel_name) + 1);
    os_strcpy((char *)agora_rtc_option.p_channel_name, channel_name);
    agora_rtc_option.audio_config.audio_data_type = CONFIG_AUDIO_CODEC_TYPE;
#if defined(CONFIG_SEND_PCM_DATA)
    agora_rtc_option.audio_config.pcm_sample_rate = CONFIG_PCM_SAMPLE_RATE;
    agora_rtc_option.audio_config.pcm_channel_num = CONFIG_PCM_CHANNEL_NUM;
#endif
    agora_rtc_option.p_token = CONFIG_AGORA_TOKEN;
    agora_rtc_option.uid = CONFIG_AGORA_UID;
    ret = bk_agora_rtc_start(&agora_rtc_option);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, bk_agora_rtc_start fail, ret:%d\n", __func__, __LINE__, ret);
        goto exit;
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

    LOGI("-----agora_rtc_join_channel success-----\n");

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
    /* free audio sources */
    if (audio_en)
    {
        audio_turn_off();
    }

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

static bk_err_t agora_stop(void)
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
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    cli_agora_rtc_help();
}

void cli_agora_rtc_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
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
            agoora_tx_mic_data_flag = true;
        }
        else
        {
            agoora_tx_mic_data_flag = false;
        }
    }
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    cli_agora_rtc_help();
}

/* call this api when wifi autoconnect */
void agora_auto_run(void)
{
    sprintf(agora_appid, "%s", CONFIG_AGORA_APP_ID);
    sprintf(channel_name, "%s", CONFIG_CHANNEL_NAME);

    audio_en = true;
    video_en = false;
    agora_start();
}

