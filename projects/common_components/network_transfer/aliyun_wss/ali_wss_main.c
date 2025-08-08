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
#include "video_engine.h"
#include "app_main.h"
#include "app_event.h"
#include "ali_wss.h"

#define TAG "ALI_WSS"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_USE_G722_CODEC || CONFIG_USE_OPUS_CODEC
#define AUDIO_SAMP_RATE         (16000)
#else
#define AUDIO_SAMP_RATE         (8000)
#endif
#define AEC_ENABLE              (1)

extern bool smart_config_running;

bool g_connected_flag = false;
static bool audio_en = false;
static bool video_en = false;
static beken_thread_t  wss_thread_hdl = NULL;
static beken_semaphore_t wss_sem = NULL;
bool wss_running = false;

DummyHandle *global_handle;
DummyHandle *get_client_handle(void)
{
	return global_handle;
}

void ali_wss_event_handler(void* event_handler_arg, char *event_base, int32_t event_id, void* event_data)
{
	bk_websocket_event_data_t *data = (bk_websocket_event_data_t *)event_data;
	transport client = (transport)event_handler_arg;
	if (client == NULL)
		LOGE("WebSocket handle null\r\n");
	switch (event_id) {
		case WEBSOCKET_EVENT_CONNECTED:
			LOGE("Connected to WebSocket server\r\n");
			global_handle->disconnecting_count = 0;
			g_connected_flag = true;
			network_reconnect_stop_timeout_check();
			app_event_send_msg(APP_EVT_AGENT_JOINED, 0);
			smart_config_running = false;
			break;
		case WEBSOCKET_EVENT_CLOSED:
			LOGE("WEBSOCKET_EVENT_CLOSED\r\n");
			if (global_handle) {
				global_handle->clientHandler = NULL;
			}
        case WEBSOCKET_EVENT_DISCONNECTED:
			LOGE("Disconnected from WebSocket server\r\n");
			g_connected_flag = false;
			if (global_handle) {
				global_handle->disconnecting_count++;
				if (global_handle->disconnecting_count == 1)
					app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
			}
			break;
        case WEBSOCKET_EVENT_DATA:
			LOGD("data from WebSocket server, len:%d op:%d\r\n", data->data_len, data->op_code);
			c_mmi_analyze_recv_data(data->op_code, (uint8_t *)data->data_ptr, data->data_len);
			break;
		default:
			break;
	}
}

#define CONFIG_MMI_MODE C_MMI_MODE_DUPLEX
int32_t _mmi_event_callback(uint32_t event, void *param)
{
    switch (event) {
        case C_MMI_EVENT_USER_CONFIG:
			LOGE("%s, C_MMI_EVENT_USER_CONFIG\n", __func__);
            //用户对于sdk的配置应该在该事件回调中实现，如音频缓冲区大小、工作模式、音色等
            // 配置mmi音频数据传输模式
            c_mmi_set_audio_mode_ringbuffer(500 * 1024, 900 * 1024);
            // 配置工作模式
            c_mmi_set_work_mode(CONFIG_MMI_MODE);
            // 配置文本选项
            c_mmi_set_text_mode(C_MMI_TEXT_MODE_BOTH);
            // 配置音色
            c_mmi_set_voice_id("longxiaochun_v2");
            break;
        case C_MMI_EVENT_DATA_INIT:
			LOGE("%s, C_MMI_EVENT_DATA_INIT\n", __func__);
            //当SDK完成初始化后触发该事件，可在该事件回调中开始建立业务连接
            // mmi data ready, 开始网络连接
			if ((global_handle = dummy_wss_init(&general_audio)) == NULL)
			{
				LOGE("%s, wss init fail\n", __func__);
			}
            break;
        case C_MMI_EVENT_SPEECH_START:
			LOGE("%s, C_MMI_EVENT_SPEECH_START\n", __func__);
            //当SDK开始进行音频上行时触发此事件
            if (dummy_record_work(global_handle) == 0) {
				dummy_record_start(global_handle);
            }
			dummy_play_stop(global_handle);
            break;
        case C_MMI_EVENT_ASR_START:
            LOGE("%s, C_MMI_EVENT_ASR_START\n", __func__);
            break;
        case C_MMI_EVENT_ASR_INCOMPLETE:
        {
            LOGE("%s, C_MMI_EVENT_ASR_INCOMPLETE\n", __func__);
            //此事件返回尚未完成ASR的文本数据（全量）
            char *text = param;
            dummy_lcd_show("ASR [%s]", text);
            break;
        }
        case C_MMI_EVENT_ASR_COMPLETE:
        {
            LOGE("%s, C_MMI_EVENT_ASR_COMPLETE\n", __func__);
            //此事件返回完成ASR的全部文本数据（全量）
            char *text = param;
            dummy_lcd_show("ASR C [%s]", text);
            break;
        }
        case C_MMI_EVENT_ASR_END:
			LOGE("%s, C_MMI_EVENT_ASR_END\n", __func__);
            //当ASR结束时触发此事件
            break;

        case C_MMI_EVENT_LLM_INCOMPLETE:
        {
            LOGE("%s, C_MMI_EVENT_LLM_INCOMPLETE\n", __func__);
            //此事件返回尚未处理完成的LLM文本数据（全量）
            char *text = param;
            dummy_lcd_show("LLM [%s]", text);
            break;
        }
        case C_MMI_EVENT_LLM_COMPLETE:
        {
            LOGE("%s, C_MMI_EVENT_LLM_COMPLETE\n", __func__);
            //此事件返回处理完成的LLM全部文本数据（全量）
            char *text = param;
            dummy_lcd_show("LLM C [%s]", text);
            break;
        }

        case C_MMI_EVENT_TTS_START:
			LOGE("%s, C_MMI_EVENT_TTS_START\n", __func__);
            //当开始音频下行时触发此事件
            dummy_play_start(global_handle);
            break;
        case C_MMI_EVENT_TTS_END:
			LOGE("%s, C_MMI_EVENT_TTS_END\n", __func__);
            //当音频完成下行时触发此事件
            dummy_play_stop(global_handle);
            break;
        default:
            break;
    }

    return UTIL_SUCCESS;
}

void ali_wss_main(void)
{
    bk_err_t ret = BK_OK;
#ifndef CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
    if (audio_en)
    {
        ret = audio_turn_on();
        if (ret != BK_OK)
        {
            LOGE("%s, %d, audio turn on fail, ret:%d\n", __func__, __LINE__, ret);
            goto exit;
        }
    }
#endif

	audio_tras_register_tx_data_func(dummy_record_data_send);
    wss_running = true;
    rtos_set_semaphore(&wss_sem);

    /* wait until we join channel successfully */
    while (!g_connected_flag)
    {
        if (!wss_running)
        {
            goto exit;
        }
        rtos_delay_milliseconds(100);
    }

    LOGI("-----beken_join_aliyun_wss success-----\r\n");

    /* turn on video */
    if (video_en)
    {
        ret = video_turn_on();
        if (ret != BK_OK)
        {
            LOGE("%s, %d, video turn on fail, ret:%d\n", __func__, __LINE__, ret);
            goto exit;
        }
    }

    while (wss_running)
    {
        rtos_delay_milliseconds(100);
    }
	LOGE("%s, %d exit\n", __func__, __LINE__);
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

	dummy_wss_destroy(global_handle);
	global_handle = NULL;
    audio_en = false;
    video_en = false;

    g_connected_flag = false;

    /* delete task */
    wss_thread_hdl = NULL;

    wss_running = false;

    rtos_set_semaphore(&wss_sem);

    rtos_delete_thread(NULL);
}

bk_err_t ali_wss_stop(void)
{
    if (!wss_running)
    {
        LOGI("beken rtc not start\n");
        return BK_OK;
    }

    wss_running = false;

    rtos_get_semaphore(&wss_sem, BEKEN_NEVER_TIMEOUT);

    rtos_deinit_semaphore(&wss_sem);
    wss_sem = NULL;

    return BK_OK;
}

static bk_err_t ali_wss_start(void)
{
    bk_err_t ret = BK_OK;

    if (wss_running)
    {
        LOGI("beken rtc already start, Please close and then reopens\n");
        return BK_FAIL;
    }

    ret = rtos_init_semaphore(&wss_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_create_thread(&wss_thread_hdl,
                             4,
                             "ali_wss",
                             (beken_thread_function_t)ali_wss_main,
                             6 * 1024,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create beken app task fail, ret:%d\n", __func__, __LINE__, ret);
        wss_thread_hdl = NULL;
        goto fail;
    }

    rtos_get_semaphore(&wss_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("create ali_wss_main task complete\n");

    return BK_OK;

fail:
    if (wss_sem)
    {
        rtos_deinit_semaphore(&wss_sem);
        wss_sem = NULL;
    }
    return BK_FAIL;
}

/* call this api when wifi autoconnect */
void ali_auto_run(uint8_t reset)
{
	LOGI("%s, reset:%d\n", __func__, reset);
    if (bk_sconf_wakeup_agent(reset)) {
        LOGE("%s, wake up agent fail!\n", __func__);
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
        return;
    }

    if (!wss_running)
    {
       // bk_genie_wakeup_agent();
        audio_en = true;
        video_en = false;
        ali_wss_start();
    }
}

static void cli_ali_wss_help(void)
{
    LOGI("ali_test {start|stop appid video_en channel_name}\n");
    LOGI("ali_debug {dump_mic_data value}\n");
}

void cli_ali_wss_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{

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
        ali_wss_start();
    }
    else if (os_strcmp(argv[1], "stop") == 0)
    {
        ali_wss_stop();
    }
    /*else if (os_strcmp(argv[1], "test") == 0)
    {
        aliyun_sdk_test();
    }*/
    else if (os_strcmp(argv[1], "erase") == 0)
    {
        util_storage_erase();
    }
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    cli_ali_wss_help();
}

