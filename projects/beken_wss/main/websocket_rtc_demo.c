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
#include "beken_config.h"
#include "beken_rtc.h"
#include "audio_transfer.h"
#include "aud_intf.h"
#include "aud_intf_types.h"
#include <driver/media_types.h>
#include <driver/lcd.h>
#include <modules/wifi.h>
#include "modules/wifi_types.h"
#include "media_app.h"
#include "lcd_act.h"
#include "components/bk_uid.h"
#include "aud_tras.h"

#if CONFIG_NETWORK_AUTO_RECONNECT
#include "bk_genie_smart_config.h"
#endif
#include "app_event.h"
#include "cJSON.h"

#define TAG "WS_MAIN"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
extern bool agoora_rx_spk_data_flag;
#endif//CONFIG_DEBUG_DUMP


//#define AGORA_RX_SPK_DATA_DUMP

#ifdef AGORA_RX_SPK_DATA_DUMP
#include "uart_util.h"
static uart_util_t g_agora_spk_uart_util = {0};
#define AGORA_RX_SPK_DATA_DUMP_UART_ID            (1)
#define AGORA_RX_SPK_DATA_DUMP_UART_BAUD_RATE     (2000000)

#define AGORA_RX_SPK_DATA_DUMP_OPEN()                        uart_util_create(&g_agora_spk_uart_util, AGORA_RX_SPK_DATA_DUMP_UART_ID, AGORA_RX_SPK_DATA_DUMP_UART_BAUD_RATE)
#define AGORA_RX_SPK_DATA_DUMP_CLOSE()                       uart_util_destroy(&g_agora_spk_uart_util)
#define AGORA_RX_SPK_DATA_DUMP_DATA(data_buf, len)           uart_util_tx_data(&g_agora_spk_uart_util, data_buf, len)
#else
#define AGORA_RX_SPK_DATA_DUMP_OPEN()
#define AGORA_RX_SPK_DATA_DUMP_CLOSE()
#define AGORA_RX_SPK_DATA_DUMP_DATA(data_buf, len)
#endif

#if CONFIG_USE_G722_CODEC || CONFIG_USE_OPUS_CODEC
#define AUDIO_SAMP_RATE         (16000)
#else
#define AUDIO_SAMP_RATE         (8000)
#endif
#define AEC_ENABLE              (1)

bool g_connected_flag = false;
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
    .info.resolution.width  = 640,//1280,//,
    .info.resolution.height = 480,//720,//,
    .info.fps = FPS20,
#endif
};

static beken_thread_t  rtc_thread_hdl = NULL;
static beken_semaphore_t rtc_sem = NULL;
bool rtc_runing = false;
audio_info_t audio_info = {};
rtc_session *beken_rtc = NULL;
rtc_session *__get_beken_rtc(void)
{
    return beken_rtc;
}

extern bool smart_config_running;
extern uint32_t volume;
extern uint32_t g_volume_gain[SPK_VOLUME_LEVEL];

#if CONFIG_WIFI_ENABLE
extern void rwnxl_set_video_transfer_flag(uint32_t video_transfer_flag);
#else
#define rwnxl_set_video_transfer_flag(...)
#endif

static void cli_beken_rtc_help(void)
{
    LOGI("rtc_test {start|stop appid video_en channel_name}\n");
    LOGI("rtc_debug {dump_mic_data value}\n");
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
            LOGE("%s, failed\r\n, __func__");
        }
    }
    else
    {
        LOGI("%s, %d\r\n", __func__, state);
    }
}
#endif

void app_media_read_frame_callback(frame_buffer_t *frame)
{
    video_frame_info_t info = { 0 };

    if (false == g_connected_flag)
    {
        /* beken rtc is not running, do not send video. */
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

    bk_rtc_video_data_send((uint8_t *)frame->frame, (size_t)frame->length, &info);

    /* send two frame images per second */
    rtos_delay_milliseconds(500);
}
 
bk_err_t video_turn_off(void)
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

bk_err_t audio_turn_off(void)
{
    bk_err_t ret =  BK_OK;
    LOGI("%s\n", __func__);

    /* stop voice */
    ret = bk_aud_intf_voc_stop();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, voice stop fail, ret:%d\n", __func__, __LINE__, ret);
    }

    /* deinit vioce */
    ret = bk_aud_intf_voc_deinit();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, voice deinit fail, ret:%d\n", __func__, __LINE__, ret);
    }

    bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_NULL);

    ret = bk_aud_intf_drv_deinit();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, aud_intf driver deinit fail, ret:%d\n", ret);
    }

    audio_tras_deinit();

    AGORA_RX_SPK_DATA_DUMP_CLOSE();

    return BK_OK;
}

bk_err_t audio_turn_on(void)
{
    bk_err_t ret =  BK_OK;
    LOGI("%s\n", __func__);

    AGORA_RX_SPK_DATA_DUMP_OPEN();

    aud_intf_drv_setup_t aud_intf_drv_setup = DEFAULT_AUD_INTF_DRV_SETUP_CONFIG();
    aud_intf_voc_setup_t aud_intf_voc_setup = DEFAULT_AUD_INTF_VOC_SETUP_CONFIG();

    audio_tras_init();

    aud_intf_drv_setup.aud_intf_tx_mic_data = send_audio_data_to_trans;
    ret = bk_aud_intf_drv_init(&aud_intf_drv_setup);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, aud_intf driver init fail, ret:%d\n", __func__, __LINE__, ret);
    }

    ret = bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_VOICE);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, aud_intf set_mode fail, ret:%d\n", __func__, __LINE__, ret);
    }
    
#if CONFIG_USE_G722_CODEC
    aud_intf_voc_setup.data_type  = AUD_INTF_VOC_DATA_TYPE_G722;
#elif CONFIG_USE_OPUS_CODEC
    aud_intf_voc_setup.data_type  = AUD_INTF_VOC_DATA_TYPE_OPUS;
    aud_intf_voc_setup.aud_codec_setup_input.enc_frame_len_in_ms = 60;//60ms frame
    aud_intf_voc_setup.aud_codec_setup_input.dec_frame_len_in_ms = 60;//60ms frame
    aud_intf_voc_setup.aud_codec_setup_input.dac_samp_rate = 24000;
#else
    aud_intf_voc_setup.data_type  = AUD_INTF_VOC_DATA_TYPE_G711A;
#endif
    aud_intf_voc_setup.spk_mode   = AUD_DAC_WORK_MODE_DIFFEN;
    aud_intf_voc_setup.aec_enable = AEC_ENABLE;
    aud_intf_voc_setup.samp_rate  = AUDIO_SAMP_RATE;
#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
    aud_intf_voc_setup.mic_gain   = 0x30;
#else
    aud_intf_voc_setup.mic_gain   = 0x3F;
#endif
    aud_intf_voc_setup.spk_gain   = g_volume_gain[volume];
    aud_intf_voc_setup.mic_type = AUD_INTF_MIC_TYPE_BOARD;
    aud_intf_voc_setup.spk_type = AUD_INTF_MIC_TYPE_BOARD;

    ret = bk_aud_intf_voc_init(aud_intf_voc_setup);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("bk_aud_intf_voc_init fail, ret:%d \r\n", ret);
    }

#if CONFIG_USE_G722_CODEC
	rtc_fill_audio_info(&audio_info, "g722", 16000, 16000, 20, 20, 160);
#elif CONFIG_USE_OPUS_CODEC
    rtc_fill_audio_info(&audio_info, "opus", aud_intf_voc_setup.aud_codec_setup_input.adc_samp_rate,
		aud_intf_voc_setup.aud_codec_setup_input.dac_samp_rate,
		aud_intf_voc_setup.aud_codec_setup_input.enc_frame_len_in_ms, aud_intf_voc_setup.aud_codec_setup_input.dec_frame_len_in_ms,
		bk_aud_get_dec_input_size_in_byte());
#endif

    ret = bk_aud_intf_voc_start();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("bk_aud_intf_voc_start fail, ret:%d \r\n", ret);
    }

    return BK_OK;
}

int rtc_user_audio_rx_data_handle(unsigned char *data, unsigned int size, const audio_frame_info_t *info_ptr)
{
    bk_err_t ret = BK_OK;

    #if CONFIG_DEBUG_DUMP
    if(agoora_rx_spk_data_flag)
    {
        //AGORA_RX_SPK_DATA_DUMP_DATA(data, size);
        #if 0
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_NUM(DUMP_TYPE_AGORA_RX_SPK,1);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW(DUMP_TYPE_AGORA_RX_SPK,0,DUMP_FILE_TYPE_G722,size);
        #else
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_AGORA_RX_SPK,0,size);
        #endif
        DEBUG_DATA_DUMP_UPDATE_HEADER_TIMESTAMP(DUMP_TYPE_AGORA_RX_SPK);
        DEBUG_DATA_DUMP_BY_UART_HEADER(DUMP_TYPE_AGORA_RX_SPK);
        DEBUG_DATA_DUMP_UPDATE_HEADER_SEQ_NUM(DUMP_TYPE_AGORA_RX_SPK);
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

void rtc_websocket_msg_handle(char *json_text, unsigned int size) {
    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL) {
        LOGE("Error: Failed to parse JSON text:%s\n", json_text);
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type == NULL) {
        LOGE("Error: Missing type field.\n");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(type->valuestring, "hello_response") == 0) {
        int ret = rtc_websocket_parse_hello(root);
		if (ret == 200) {
			g_connected_flag = true;
			network_reconnect_stop_timeout_check();
			app_event_send_msg(APP_EVT_AGENT_JOINED, 0);
			smart_config_running = false;
		}
		else {
			LOGE("join WebSocket server fail\r\n");
		}
    } else if ((strcmp(type->valuestring, "reply_text") == 0) || (strcmp(type->valuestring, "request_text") == 0)) {
        text_info_t info = {};
        info.text_type = (strcmp(type->valuestring, "request_text") == 0) ? 0:1;
        rtc_websocket_parse_text(&info, root);
        LOGE("text: type:%s data:%s\n", info.text_type ? "reply":"request", info.text_data);
    } else {
        LOGE("Error: Unknown type: %s\n", type->valuestring);
    }
    cJSON_Delete(root);
}

void rtc_websocket_event_handler(void* event_handler_arg, char *event_base, int32_t event_id, void* event_data)
{
	bk_websocket_event_data_t *data = (bk_websocket_event_data_t *)event_data;
	transport client = (transport)event_handler_arg;
	switch (event_id) {
		case WEBSOCKET_EVENT_CONNECTED:
			LOGE("Connected to WebSocket server\r\n");
			rtc_websocket_send_text(client, (void *)(&audio_info), BEKEN_RTC_SEND_HELLO);
			break;
        case WEBSOCKET_EVENT_DISCONNECTED:
			LOGE("Disconnected from WebSocket server\r\n");
			g_connected_flag = false;
			app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
			break;
        case WEBSOCKET_EVENT_DATA:
			LOGD("data from WebSocket server, len:%d op:%d\r\n", data->data_len, data->op_code);
			if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
				#if CONFIG_USE_G722_CODEC
				rtc_websocket_audio_receive_data(__get_beken_rtc(), (uint8_t *)data->data_ptr, data->data_len);
				#elif CONFIG_USE_OPUS_CODEC
				rtc_websocket_audio_receive_data_opus(__get_beken_rtc(), (uint8_t *)data->data_ptr, data->data_len);
				#else
				#endif
			}
			else if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
				rtc_websocket_msg_handle(data->data_ptr, data->data_len);
			}
			break;
		default:
			break;
	}
}

void beken_rtc_main(void)
{
    bk_err_t ret = BK_OK;
    memory_free_show();

	websocket_client_input_t websocket_cfg = {0};
	websocket_cfg.uri = "wss://ai.aclsemi.com:9015/xiaozhi/v1/";
	websocket_cfg.ws_event_handler = rtc_websocket_event_handler;
	rtc_session *rtc_session = rtc_websocket_create(&websocket_cfg, rtc_user_audio_rx_data_handle, &audio_info);
    if (rtc_session == NULL)
    {
        LOGE("rtc_websocket_create fail\r\n");
        return;
    }
	beken_rtc = rtc_session;

    rtc_runing = true;

    rtos_set_semaphore(&rtc_sem);

    /* wait until we join channel successfully */
    while (!g_connected_flag)
    {
        if (!rtc_runing)
        {
            goto exit;
        }
        rtos_delay_milliseconds(100);
    }

    LOGI("-----beken_rtc_join_channel success-----\r\n");

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
	
    while (rtc_runing)
    {
        rtos_delay_milliseconds(100);
    }

exit:
    /* free video sources */
    if (video_en)
    {
        video_turn_off();
    }

	rtc_websocket_stop(__get_beken_rtc());
	beken_rtc = NULL;
    audio_en = false;
    video_en = false;

    g_connected_flag = false;

    /* delete task */
    rtc_thread_hdl = NULL;

    rtc_runing = false;

    rtos_set_semaphore(&rtc_sem);

    rtos_delete_thread(NULL);
}

bk_err_t beken_rtc_stop(void)
{
    if (!rtc_runing)
    {
        LOGI("beken rtc not start\n");
        return BK_OK;
    }

    rtc_runing = false;

    rtos_get_semaphore(&rtc_sem, BEKEN_NEVER_TIMEOUT);

    rtos_deinit_semaphore(&rtc_sem);
    rtc_sem = NULL;

    return BK_OK;
}

static bk_err_t beken_rtc_start(void)
{
    bk_err_t ret = BK_OK;

    if (rtc_runing)
    {
        LOGI("beken rtc already start, Please close and then reopens\n");
        return BK_FAIL;
    }

    ret = rtos_init_semaphore(&rtc_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_create_thread(&rtc_thread_hdl,
                             4,
                             "beken_rtc",
                             (beken_thread_function_t)beken_rtc_main,
                             6 * 1024,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create beken app task fail, ret:%d\n", __func__, __LINE__, ret);
        rtc_thread_hdl = NULL;
        goto fail;
    }

    rtos_get_semaphore(&rtc_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("create beken app task complete\n");

    return BK_OK;

fail:

    if (rtc_sem)
    {
        rtos_deinit_semaphore(&rtc_sem);
        rtc_sem = NULL;
    }

    return BK_FAIL;
}

/* call this api when wifi autoconnect */
void beken_auto_run(void)
{
    if (!rtc_runing)
    {
       // bk_genie_wakeup_agent();
        audio_en = true;
        video_en = false;
        beken_rtc_start();
    }
}

void cli_beken_rtc_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc < 3)
    {
        goto cmd_fail;
    }

    /* audio test */
    if (os_strcmp(argv[1], "start") == 0)
    {
        audio_en = true;
        if (os_strtoul(argv[2], NULL, 10))
        {
            video_en = true;
        }
        else
        {
            video_en = false;
        }
        beken_rtc_start();
    }
    else if (os_strcmp(argv[1], "stop") == 0)
    {
        beken_rtc_stop();
    }
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    cli_beken_rtc_help();
}

