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
#include "volc_config.h"
#include "volc_rtc.h"
#include "volc_device_finger_print.h"
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
#include "bk_factory_config.h"
#if CONFIG_BK_SMART_CONFIG
#include "bk_smart_config.h"
#endif
// #include "bk_smart_config_volc_adapter.h"
#include "app_event.h"
#include "audio_process.h"
#include "volc_memory.h"
#include "cJSON.h"
#include "mbedtls/platform.h"
#include "cli.h"
#if CONFIG_BK_DEV_STARTUP_AGENT
#include "RtcBotUtils.h"
#endif
#include "video_engine.h"

#include <driver/aon_rtc.h>

#define TAG "volc_main"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
extern bool rx_spk_data_flag;
#endif//CONFIG_DEBUG_DUMP

extern rtc_room_info_t *volc_room_info;
//#define BYTE_RX_SPK_DATA_DUMP

#ifdef BYTE_RX_SPK_DATA_DUMP
#include "uart_util.h"
static uart_util_t g_byte_spk_uart_util = {0};
#define BYTE_RX_SPK_DATA_DUMP_UART_ID            (1)
#define BYTE_RX_SPK_DATA_DUMP_UART_BAUD_RATE     (2000000)

#define BYTE_RX_SPK_DATA_DUMP_OPEN()                        uart_util_create(&g_byte_spk_uart_util, BYTE_RX_SPK_DATA_DUMP_UART_ID, BYTE_RX_SPK_DATA_DUMP_UART_BAUD_RATE)
#define BYTE_RX_SPK_DATA_DUMP_CLOSE()                       uart_util_destroy(&g_byte_spk_uart_util)
#define BYTE_RX_SPK_DATA_DUMP_DATA(data_buf, len)           uart_util_tx_data(&g_byte_spk_uart_util, data_buf, len)
#else
#define BYTE_RX_SPK_DATA_DUMP_OPEN()
#define BYTE_RX_SPK_DATA_DUMP_CLOSE()
#define BYTE_RX_SPK_DATA_DUMP_DATA(data_buf, len)
#endif  //BYTE_RX_SPK_DATA_DUMP

#define VIDEO_FRAME_INTERVAL            500

bool g_connected_flag = false;
bool g_agent_offline = true;
static bool audio_en = false;
static bool video_en = false;

static beken_thread_t  byte_thread_hdl = NULL;
static beken_semaphore_t byte_sem = NULL;
bool byte_runing = false;
static byte_rtc_config_t byte_rtc_config = DEFAULT_BYTE_RTC_CONFIG();
static byte_rtc_option_t byte_rtc_option = DEFAULT_BYTE_RTC_OPTION();

static uint32_t g_target_bps = BANDWIDTH_ESTIMATE_MIN_BITRATE;
extern bool smart_config_running;
extern uint32_t volume;
extern uint32_t g_volume_gain[SPK_VOLUME_LEVEL];
extern app_aud_para_t app_aud_cust_para;

#if 0
bool agoora_tx_mic_data_flag = false;
#if CONFIG_SYS_CPU1
extern bool aec_all_data_flag;
#endif
#endif


static void cli_byte_rtc_help(void)
{
    LOGI("byte_test {start|stop appid video_en channel_name}\n");
    LOGI("byte_debug {dump_mic_data value}\n");
}

static void byte_print_finger()
{
    volc_string_t finger;
    volc_string_init(&finger);
    volc_get_device_finger_print(&finger);
    LOGI("finger length:%d capacity:%d buffer:%s\r\n", finger.length,  finger.capacity, finger.buffer);
    volc_string_deinit(&finger);
}

static void byte_rtc_user_notify_msg_handle(byte_rtc_msg_t *p_msg)
{
    switch (p_msg->code)
    {
        case BYTE_RTC_MSG_JOIN_CHANNEL_SUCCESS:
            g_connected_flag = true;
            LOGI("Join channel success.\n");
            break;
        case BYTE_RTC_MSG_REJOIN_CHANNEL_SUCCESS:
            g_connected_flag = true;
            LOGI("Rejoin channel success.\n");
            if (g_agent_offline == false)
                network_reconnect_stop_timeout_check();
            app_event_send_msg(APP_EVT_RTC_REJOIN_SUCCESS, 0);
            break;
        case BYTE_RTC_MSG_USER_JOINED:
            LOGI("User Joined.\n");
            network_reconnect_stop_timeout_check();
            app_event_send_msg(APP_EVT_AGENT_JOINED, 0);
            g_agent_offline = false;
            smart_config_running = false;
            break;
        case BYTE_RTC_MSG_USER_OFFLINE:
            LOGI("User Offline.\n");
            g_agent_offline = true;
            app_event_send_msg(APP_EVT_AGENT_OFFLINE, 0);
            if (g_connected_flag == true)
               app_event_send_msg(APP_EVT_AGENT_DEVICE_REMOVE, 0);
            break;
        case BYTE_RTC_MSG_CONNECTION_LOST:
            LOGE("Lost connection. Please check wifi status.\n");
            g_connected_flag = false;
            app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
            break;
        case BYTE_RTC_MSG_INVALID_APP_ID:
            LOGE("Invalid App ID. Please double check.\n");
            break;
        case BYTE_RTC_MSG_INVALID_CHANNEL_NAME:
            LOGE("Invalid channel name. Please double check.\n");
            break;
        case BYTE_RTC_MSG_INVALID_TOKEN:
        case BYTE_RTC_MSG_TOKEN_EXPIRED:
            LOGE("Invalid token. Please double check.\n");
            break;
        case BYTE_RTC_MSG_BWE_TARGET_BITRATE_UPDATE:
            g_target_bps = p_msg->bwe.target_bitrate;
            break;
        case BYTE_RTC_MSG_KEY_FRAME_REQUEST:
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


static int byte_rtc_user_audio_rx_data_handle(unsigned char *data, unsigned int size, audio_data_type_e data_type)
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

static void app_media_read_frame_callback(frame_buffer_t *frame)
{
    video_frame_info_t info = { 0 };
    static uint64_t before = 0, curr = 0;
    curr = bk_aon_rtc_get_ms();

    if (false == g_connected_flag)
    {
        /* volc rtc is not running, do not send video. */
        return;
    }

    info.stream_type = VIDEO_STREAM_HIGH;
    if (frame->fmt == PIXEL_FMT_H264)
    {
        if ((frame->h264_type & (1 << H264_NAL_I_FRAME)) == 0)
        {
            //LOGI("%s, ####not i frame, 0x%x%08x - 0x%x%08x : 0x%x%08x###\n", __func__, (uint32_t)(curr>>32), (uint32_t)curr, (uint32_t)(before>>32), (uint32_t)before, (uint32_t)((curr - before)>>32), (uint32_t)(curr - before));
            return;
        }
        info.data_type = VIDEO_DATA_TYPE_H264;
        info.frame_type = VIDEO_FRAME_AUTO_DETECT;
        info.frame_rate = 1000/VIDEO_FRAME_INTERVAL;
    }
    else
    {
        LOGE("not support format: %d \r\n", frame->fmt);
    }

    if (curr > before && curr - before >= VIDEO_FRAME_INTERVAL)
    {
        //LOGI("##########send frame: 0x%x%08x - 0x%x%08x : 0x%x%08x######################\n", (uint32_t)(curr>>32), (uint32_t)curr, (uint32_t)(before>>32), (uint32_t)before, (uint32_t)((curr - before)>>32), (uint32_t)(curr - before));

#if (CONFIG_IMAGE_DEBUG_DUMP)
        do {
#endif
            bk_byte_rtc_video_data_send((uint8_t *)frame->frame, (size_t)frame->length, &info);
            /* send two frame images per second */
#if (CONFIG_IMAGE_DEBUG_DUMP)
        } while (video_lock);
#endif
        before = curr;
    }
}

void byte_main(void)
{
    bk_err_t ret = BK_OK;

    memory_free_show();

    if (!volc_room_info) {
        LOGE("room info empty!\r\n");
        return;
    }

    if (strlen(volc_room_info->app_id) == 0)
    {
        LOGE("app_id empty!\r\n");
        return;
    }
    //byte_print_finger();
    mbedtls_platform_set_calloc_free(volc_calloc, volc_free);

    cJSON_Hooks hook = {volc_malloc, volc_free};
    cJSON_InitHooks(&hook);

    //service_opt.license_value[0] = '\0';
    byte_rtc_config.p_appid = (char *)psram_malloc(strlen(volc_room_info->app_id) + 1);
    os_strcpy((char *)byte_rtc_config.p_appid, volc_room_info->app_id);
    byte_rtc_config.log_level = BYTE_RTC_LOG_LEVEL_INFO;

    audio_tras_register_tx_data_func(bk_byte_rtc_audio_data_send);
    video_register_tx_data_func(app_media_read_frame_callback);

    ret = bk_byte_rtc_create(&byte_rtc_config, (byte_rtc_msg_notify_cb)byte_rtc_user_notify_msg_handle);
    if (ret != BK_OK)
    {
        LOGI("bk_byte_rtc_create fail \r\n");
    }
    // LOGI("-----start byte rtc process-----\r\n");

    byte_rtc_option.room = volc_room_info;

    byte_rtc_option.audio_data_type = bk_byte_rtc_audio_codec_type_mapping(bk_aud_get_encoder_type());

    ret = bk_byte_rtc_start(&byte_rtc_option);
    if (ret != BK_OK)
    {
        LOGE("bk_byte_rtc_start fail, ret:%d \r\n", ret);
        return;
    }

    byte_runing = true;

    rtos_set_semaphore(&byte_sem);

    /* wait until we join channel successfully */
    while (!g_connected_flag)
    {
        // memory_free_show();
        //        rtos_dump_task_runtime_stats();
        if (!byte_runing)
        {
            goto exit;
        }
        rtos_delay_milliseconds(100);
    }

    LOGI("-----byte_rtc_join_channel success-----\r\n");

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
    ret = bk_byte_rtc_register_audio_rx_handle((byte_rtc_audio_rx_data_handle)byte_rtc_user_audio_rx_data_handle);
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

    while (byte_runing)
    {
        rtos_delay_milliseconds(100);
        // memory_free_show();
        // rtos_dump_task_runtime_stats();
    }

exit:
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
    /* deregister callback to handle audio data received from byte rtc */
    bk_byte_rtc_register_audio_rx_handle(NULL);
#else
    /* free audio  */
    if (audio_en)
    {
        audio_en = false;
        audio_turn_off();
    }
#endif

    /* free video sources */
    if (video_en)
    {
        video_en = false;
        video_turn_off();
    }

    /* free byte */
    /* stop byte rtc */
    bk_byte_rtc_stop();

    /* destory byte rtc */
    bk_byte_rtc_destroy();

    if (byte_rtc_config.p_appid)
    {
        psram_free((char *)byte_rtc_config.p_appid);
        byte_rtc_config.p_appid = NULL;
    }

    if (volc_room_info)
    {
        psram_free((char *)volc_room_info);
        volc_room_info = NULL;
    }

    g_connected_flag = false;

    /* delete task */
    byte_thread_hdl = NULL;

    byte_runing = false;

    rtos_set_semaphore(&byte_sem);
    LOGE("byte_main exit\n");
    rtos_delete_thread(NULL);
}

bk_err_t byte_stop(void)
{
    LOGI("%s, %d\n", __func__, __LINE__);
    if (!byte_runing)
    {
        LOGI("byte not start\n");
        return BK_OK;
    }

    byte_runing = false;

    rtos_get_semaphore(&byte_sem, BEKEN_NEVER_TIMEOUT);

    rtos_deinit_semaphore(&byte_sem);
    byte_sem = NULL;
    LOGI("%s, %d\n", __func__, __LINE__);
    return BK_OK;
}

static bk_err_t byte_start(void)
{
    bk_err_t ret = BK_OK;

    LOGI("%s, %d\n", __func__, __LINE__);
    if (byte_runing)
    {
        LOGI("byte already start, Please close and then reopens\n");
        return BK_FAIL;
    }

    ret = rtos_init_semaphore(&byte_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_create_thread(&byte_thread_hdl,
                             4,
                             "byte",
                             (beken_thread_function_t)byte_main,
                             6 * 1024,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create byte app task fail, ret:%d\n", __func__, __LINE__, ret);
        byte_thread_hdl = NULL;
        goto fail;
    }

    rtos_get_semaphore(&byte_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("create byte app task complete\n");

    return BK_OK;

fail:

    if (byte_sem)
    {
        rtos_deinit_semaphore(&byte_sem);
        byte_sem = NULL;
    }

    LOGI("%s, %d\n", __func__, __LINE__);
    return BK_FAIL;
}


void cli_byte_rtc_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc < 2)
    {
        goto cmd_fail;
    }

    /* audio test */
    if (os_strcmp(argv[1], "start") == 0)
    {
        // if (argc < 4)
        // {
        //     goto cmd_fail;
        // }
        // audio_en = true;
        // sprintf(byte_appid, "%s", argv[2]);
        // if (os_strtoul(argv[3], NULL, 10))
        // {
        //     video_en = true;
        // }
        // else
        // {
        //     video_en = false;
        // }

        // if (argc >= 5)
        // {
        //     sprintf(channel_name, "%s", argv[4]);
        // }
        // else
        // {
        //     unsigned char uid[32] = {0};
        //     bk_uid_get_data(uid);
        //     sprintf(channel_name, "%s", uid);
        // }

        byte_start();
    }
    else if (os_strcmp(argv[1], "stop") == 0)
    {
        byte_stop();
    }
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    cli_byte_rtc_help();
}
#if 0
void cli_byte_rtc_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
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
    cli_byte_rtc_help();
}
#endif
/* call this api when wifi autoconnect */
void byte_restart(bool enable_video)
{
    LOGI("%s, %d\n", __func__, __LINE__);
    if (!byte_runing)
    {
        audio_en = true;
        video_en = enable_video;
        byte_start();
    }
    LOGI("%s, %d\n", __func__, __LINE__);
}

void byte_auto_run(uint8_t reset)
{
    if (bk_sconf_wakeup_agent(reset)) {
        LOGE("%s, wake up agent fail!\n", __func__);
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
        return;
    }
    bk_sconf_sync_flash();
    if (!byte_runing)
    {
        audio_en = true;
#if CONFIG_VOLC_ENABLE_VISION_BY_DEFAULT
        video_en = true;
#else
        video_en = false;
#endif
        byte_start();
    }
}

#define BYTE_RTC_CMD_CNT   (sizeof(s_byte_rtc_commands) / sizeof(struct cli_command))

static const struct cli_command s_byte_rtc_commands[] =
{
    {"volc_test", "volc_test ...", cli_byte_rtc_test_cmd},
};

int byte_rtc_cli_init(void)
{
    return cli_register_commands(s_byte_rtc_commands, BYTE_RTC_CMD_CNT);
}
