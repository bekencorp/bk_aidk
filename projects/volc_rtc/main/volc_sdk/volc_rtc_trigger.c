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
#include "audio_transfer.h"
#if CONFIG_NETWORK_AUTO_RECONNECT
#include "bk_genie_smart_config.h"
#endif
#include "app_event.h"
#include "audio_process.h"
#include "volc_memory.h"
#include "cJSON.h"
#include "mbedtls/platform.h"
#include "cli.h"
#if CONFIG_HTTP_REQUEST_AGENT
#include "RtcBotUtils.h"
#endif
#define TAG "volc_main"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
extern bool byte_rx_spk_data_flag;
#endif//CONFIG_DEBUG_DUMP


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


bool g_connected_flag = false;
bool g_agent_offline = true;
//static char byte_appid[33] = DEFAULT_RTC_APP_ID;
static char channel_name[128] = {0};
static bool audio_en = false;
static bool video_en = false;
// static media_camera_device_t camera_device =
// {

// #if defined(CONFIG_UVC_CAMERA)
//     .type = UVC_CAMERA,
//     .mode = JPEG_MODE,
//     .fmt  = PIXEL_FMT_JPEG,
//     /* expect the width and length */
//     .info.resolution.width  = 640,//640,//864,
//     .info.resolution.height = 480,
//     .info.fps = FPS25,
// #elif defined(CONFIG_DVP_CAMERA)
//     /* DVP Camera */
//     .type = DVP_CAMERA,
//     .mode = H264_MODE,//JPEG_MODE
//     .fmt  = PIXEL_FMT_H264,//PIXEL_FMT_JPEG
//     /* expect the width and length */
//     .info.resolution.width  = 640,//1280,//,
//     .info.resolution.height = 480,//720,//,
//     .info.fps = FPS20,
// #endif
// };


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
    return audio_tras_user_audio_rx_data_handle(data, size);
}

void byte_main(void)
{
    bk_err_t ret = BK_OK;
    rtc_room_info_t* room_info = NULL;

    memory_free_show();

    room_info = psram_malloc(sizeof(rtc_room_info_t));
    if (!room_info)
    {
        LOGE("room info INVALID\r\n");
        return;
    }

    mbedtls_platform_set_calloc_free(volc_calloc, volc_free);

    cJSON_Hooks hook = {volc_malloc, volc_free};
    cJSON_InitHooks(&hook);

#if CONFIG_HTTP_REQUEST_AGENT
    int start_ret = start_voice_bot(room_info);
    if (start_ret != 200) {
        LOGE("Bot start Failed, ret = %d\r\n", start_ret);
        psram_free(room_info);
        return;
    }
#else
    os_strcpy((char *)room_info->room_id, DEFAULT_ROOM_ID);
    os_strcpy((char *)room_info->uid, DEFAULT_USER_ID);
    os_strcpy((char *)room_info->app_id, DEFAULT_RTC_APP_ID);
    os_strcpy((char *)room_info->token, DEFAULT_TOKEN);
#endif
    //service_opt.license_value[0] = '\0';
    byte_rtc_config.p_appid = (char *)psram_malloc(strlen(room_info->app_id) + 1);
    os_strcpy((char *)byte_rtc_config.p_appid, room_info->app_id);
    byte_rtc_config.log_level = BYTE_RTC_LOG_LEVEL_INFO;
    audio_tras_register_tx_data_func(bk_byte_rtc_audio_data_send);
    ret = bk_byte_rtc_create(&byte_rtc_config, (byte_rtc_msg_notify_cb)byte_rtc_user_notify_msg_handle);
    if (ret != BK_OK)
    {
        LOGI("bk_byte_rtc_create fail \r\n");
    }
    // LOGI("-----start byte rtc process-----\r\n");

    byte_rtc_option.room = room_info;
    byte_rtc_option.audio_data_type = AUDIO_DATA_TYPE_OPUS;

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

    // /* turn on video */
    // if (video_en)
    // {
    //     ret = video_turn_on();
    //     if (ret != BK_OK)
    //     {
    //         LOGE("%s, %d, video turn on fail, ret:%d\n", __func__, __LINE__, ret);
    //         goto exit;
    //     }
    //     memory_free_show();
    // }

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
        audio_turn_off();
    }
#endif

    /* free video sources */
    if (video_en)
    {
        //video_turn_off();
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

    if (byte_rtc_option.room)
    {
        psram_free((char *)byte_rtc_option.room);
        byte_rtc_option.room = NULL;
    }

    audio_en = false;
    video_en = false;

    g_connected_flag = false;

    /* delete task */
    byte_thread_hdl = NULL;

    byte_runing = false;

    rtos_set_semaphore(&byte_sem);

    rtos_delete_thread(NULL);
}

bk_err_t byte_stop(void)
{
    if (!byte_runing)
    {
        LOGI("byte not start\n");
        return BK_OK;
    }

    byte_runing = false;

    rtos_get_semaphore(&byte_sem, BEKEN_NEVER_TIMEOUT);

    rtos_deinit_semaphore(&byte_sem);
    byte_sem = NULL;

    return BK_OK;
}

static bk_err_t byte_start(void)
{
    bk_err_t ret = BK_OK;

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
extern char *app_id_record;
extern char *channel_name_record;
void byte_auto_run(void)
{
#if !CONFIG_BK_BYTE_DEV_STARTUP_AGENT
    if (!channel_name_record || !app_id_record)
    {
        return;
    }
#endif
    if (bk_genie_wakeup_agent()) {
        LOGE("%s, wake up agent fail!\n", __func__);
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
        return;
    }
    //sprintf(byte_appid, "%s", app_id_record);
    sprintf(channel_name, "%s", channel_name_record);
    if (!byte_runing)
    {
        audio_en = true;
        video_en = false;
        byte_start();
    }
}
void agora_auto_run(void)
{

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