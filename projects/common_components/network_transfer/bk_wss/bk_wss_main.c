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
#include "bk_wss_config.h"
#include "bk_wss_private.h"
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
#include "media_app.h"
#include "bk_smart_config.h"
#include "app_event.h"
#include "cJSON.h"
#include <modules/audio_process.h>
#include "audio_engine.h"
#include "video_engine.h"
#include "bk_wss.h"
#include "app_main.h"


#define TAG "WS_MAIN"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
extern bool rx_spk_data_flag;
#endif//CONFIG_DEBUG_DUMP


#if CONFIG_USE_G722_CODEC || CONFIG_USE_OPUS_CODEC
#define AUDIO_SAMP_RATE         (16000)
#else
#define AUDIO_SAMP_RATE         (8000)
#endif
#define AEC_ENABLE              (1)

#if 0
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
#endif
bool g_connected_flag = false;
static bool audio_en = false;
static bool video_en = false;
static beken_thread_t  rtc_thread_hdl = NULL;
static beken_semaphore_t rtc_sem = NULL;
bool rtc_runing = false;
audio_info_t audio_info = {};
rtc_session *beken_rtc = NULL;
dialog_session_t dialog_info = {};

rtc_session *__get_beken_rtc(void)
{
    return beken_rtc;
}

extern bool smart_config_running;
extern uint32_t volume;
extern uint32_t g_volume_gain[SPK_VOLUME_LEVEL];
extern app_aud_para_t app_aud_cust_para;


#if CONFIG_WIFI_ENABLE
extern void rwnxl_set_video_transfer_flag(uint32_t video_transfer_flag);
#else
#define rwnxl_set_video_transfer_flag(...)
#endif

static void cli_beken_wss_help(void)
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

int bk_wss_user_audio_rx_data_handle(unsigned char *data, unsigned int size, const audio_frame_info_t *info_ptr)
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

int32_t bk_wss_state_event(uint32_t event, void *param)
{
	switch (event) {
		case WSS_EVENT_RECORDING_START:
			LOGE("%s, WSS_EVENT_RECORDING_START\n", __func__);
			websocket_event_send_msg(WSS_EVT_AUDIO_BUF_COMMIT, 0);
			if (wss_record_work(__get_beken_rtc()) == 0) {
				wss_record_start(__get_beken_rtc());
			}
			wss_play_stop(__get_beken_rtc());
			break;
		case WSS_EVENT_RECORDING_END:
			LOGE("%s, WSS_EVENT_RECORDING_END\n", __func__);
			if (wss_record_work(__get_beken_rtc()) != 0) {
				wss_record_stop(__get_beken_rtc());
			}
			break;
		case WSS_EVENT_LLM_COMPLETE:
			LOGE("%s, WSS_EVENT_LLM_COMPLETE\n", __func__);
			break;
		case WSS_EVENT_PLAYING_START:
			LOGE("%s, WSS_EVENT_PLAYING_START\n", __func__);
			rtc_websocket_rx_data_clean();
			wss_play_start(__get_beken_rtc());
			break;
		case WSS_EVENT_PLAYING_END:
			LOGE("%s, WSS_EVENT_PLAYING_END\n", __func__);
			wss_play_stop(__get_beken_rtc());
			break;
		default:
			break;
	}
	return BK_OK;
}

void bk_websocket_msg_handle(char *json_text, unsigned int size)
{
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
			__get_beken_rtc()->disconnecting_state = 0;
#if CONFIG_BK_WSS_TRANS_NOPSRAM
            websocket_event_send_msg(WSS_EVT_SESSION_UPDATE, 0);
#endif
		}
		else {
			LOGE("join WebSocket server fail\r\n");
		}
    } else if ((strcmp(type->valuestring, "reply_text") == 0) || (strcmp(type->valuestring, "request_text") == 0)) {
#if CONFIG_SINGLE_SCREEN_FONT_DISPLAY
		rtc_websocket_audio_receive_text(__get_beken_rtc(), (uint8_t *)json_text, size);
#else
		cJSON *text = cJSON_GetObjectItem(root, "text");
		if (text == NULL) {
			LOGE("Error: Missing required fields in [%s]\n", type->valuestring);
			return;
		}
		LOGE("receive type = %s, text = [%s]\n", (strcmp(type->valuestring, "reply_text") == 0) ? "reply" : "request", text->valuestring);
#endif
    }
#if CONFIG_BK_WSS_TRANS_NOPSRAM
    else if (strncmp(type->valuestring, "session.updated", 15) == 0) {
        LOGI("session.updated\n");
        websocket_event_send_msg(WSS_EVT_SERVER_SESSION_UPDATED, 0);
    } else if (strncmp(type->valuestring, "input_audio_buffer.committed", 28) == 0) {
        LOGI("input_audio_buffer.committed\n");
        websocket_event_send_msg(WSS_EVT_RSP_CREATE, 0);
        bk_wss_state_event(WSS_EVENT_RECORDING_END, NULL);
    } else if ((strncmp(type->valuestring, "input_audio_buffer.invalid", 26) == 0)) {
        BK_LOGW(TAG, "invalid!\n");
        bk_wss_state_event(WSS_EVENT_RECORDING_END, NULL);
    } else if (strncmp(type->valuestring, "response.created", 16) == 0) {
        LOGI("response.created\n");
        bk_wss_state_event(WSS_EVENT_PLAYING_START, NULL);
        text_info_t info = {};
        rtc_websocket_parse_request_text(&info, root);
    } else if (strncmp(type->valuestring, "response.audio.done", 19) == 0) {
        LOGI("response.audio.done\n");
        text_info_t info = {};
        parse_audio_done(&info, root);
    } else if ((strncmp(type->valuestring, "pack_pause", 10) == 0)) {
        __get_beken_rtc()->fc_is_acked = 0;
        LOGE("%s pack_pause\r\n", __func__);
    }
#endif
    else {
        LOGE("Warning: Unknown type: %s\n", type->valuestring);
    }
    cJSON_Delete(root);
}

void bk_websocket_event_handler(void* event_handler_arg, char *event_base, int32_t event_id, void* event_data)
{
	bk_websocket_event_data_t *data = (bk_websocket_event_data_t *)event_data;
	transport client = (transport)event_handler_arg;
	if (!client)
		LOGE("%s websocekt client hander NULL!\r\n", __func__);

	switch (event_id) {
		case WEBSOCKET_EVENT_CONNECTED:
			LOGE("Connected to WebSocket server\r\n");
#if !CONFIG_BK_WSS_TRANS_NOPSRAM
			rtc_websocket_send_text(__get_beken_rtc(), (void *)(&audio_info), BEKEN_RTC_SEND_HELLO);
#else
            rtc_websocket_send_text(__get_beken_rtc(), (void *)(&dialog_info), BEKEN_RTC_SEND_HELLO);
#endif
			break;
		case WEBSOCKET_EVENT_CLOSED:
			LOGE("WEBSOCKET_EVENT_CLOSED\r\n");
			if (__get_beken_rtc()) {
				__get_beken_rtc()->bk_rtc_client = NULL;
			}
        case WEBSOCKET_EVENT_DISCONNECTED:
			LOGE("Disconnected from WebSocket server\r\n");
			g_connected_flag = false;
			if(__get_beken_rtc()) {
				__get_beken_rtc()->disconnecting_state++;
				if (__get_beken_rtc()->disconnecting_state == 1)
					app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
			}
			break;
        case WEBSOCKET_EVENT_DATA:
			LOGD("data from WebSocket server, len:%d op:%d\r\n", data->data_len, data->op_code);
			if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
				rtc_websocket_audio_receive_data_general(__get_beken_rtc(), (uint8_t *)data->data_ptr, data->data_len);
			}
			else if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
				bk_websocket_msg_handle(data->data_ptr, data->data_len);
			}
			break;
		default:
			break;
	}
}

void beken_wss_main(void)
{
    bk_err_t ret = BK_OK;
    memory_free_show();
#ifndef CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
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
	websocket_client_input_t websocket_cfg = {0};
	websocket_cfg.uri = "wss://ai.aclsemi.com:9015";
	websocket_cfg.ws_event_handler = bk_websocket_event_handler;
	audio_tras_register_tx_data_func(rtc_websocket_audio_send_data);
	os_memcpy(&audio_info, &general_audio, sizeof(general_audio));
	rtc_session *rtc_session = rtc_websocket_create(&websocket_cfg, bk_wss_user_audio_rx_data_handle, &audio_info);
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
#ifndef CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
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

bk_err_t beken_wss_stop(void)
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

static bk_err_t beken_wss_start(void)
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
#if !CONFIG_BK_WSS_TRANS_NOPSRAM
    ret = rtos_create_thread(&rtc_thread_hdl,
                             4,
                             "beken_rtc",
                             (beken_thread_function_t)beken_wss_main,
                             6 * 1024,
                             NULL);
#else 
    ret = rtos_create_thread(&rtc_thread_hdl,
                             4,
                             "beken_rtc",
                             (beken_thread_function_t)beken_wss_main,
                             2 * 1024,
                             NULL);
#endif
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
        beken_wss_start();
    }
}

void cli_beken_wss_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
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
        beken_wss_start();
    }
    else if (os_strcmp(argv[1], "stop") == 0)
    {
        beken_wss_stop();
    }
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    cli_beken_wss_help();
}

