#include <common/sys_config.h>
#include <components/log.h>
#include <modules/wifi.h>
#include <components/netif.h>
#include <components/event.h>
#include <string.h>

#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>

#include "app_event.h"
#include "media_app.h"
#include "led_blink.h"

#include "countdown.h"
#include "components/bluetooth/bk_dm_bluetooth.h"
#include "boarding_service.h"
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#include "aud_intf.h"
#include "aud_intf_types.h"
#endif
#include "bat_monitor.h"
#define TAG "app_evt"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


typedef struct
{
    beken_thread_t thread;
    beken_queue_t queue;
} app_evt_info_t;

typedef struct
{
    uint32_t event;
    uint32_t param;
} app_evt_msg_t;

extern void lvgl_app_init(void);
extern void lvgl_app_deinit(void);


static app_evt_info_t app_evt_info;




bk_err_t app_event_send_msg(uint32_t event, uint32_t param)
{
    bk_err_t ret;
    app_evt_msg_t msg;

    msg.event = event;
    msg.param = param;

    ret = rtos_push_to_queue(&app_evt_info.queue, &msg, BEKEN_NO_WAIT);
    if (BK_OK != ret)
    {
        LOGE("%s, %d : %d fail \n", __func__, __LINE__, event);
        return BK_FAIL;
    }

    return BK_FAIL;
}

void app_event_asr_evt_callback(media_app_evt_type_t event, uint32_t param)
{
    LOGD("asr event callback: %x\n", event);

    /*Do not do anything blocking here */

    switch (event)
    {
        case MEDIA_APP_EVT_ASR_WAKEUP_IND:
            app_event_send_msg(APP_EVT_ASR_WAKEUP, 0);
            break;
        case MEDIA_APP_EVT_ASR_STANDBY_IND:
            app_event_send_msg(APP_EVT_ASR_STANDBY, 0);
            break;
    }
}


static void app_event_thread(beken_thread_arg_t data)
{
    int ret = BK_OK;
    uint32_t network_err = 0;
    uint32_t is_standby = 1;
    uint32_t is_network_provisioning = 0;
    uint32_t is_low_voltage_warning = 0;
    uint32_t low_voltage_warning_tick = 0;

    media_app_asr_evt_register_callback(app_event_asr_evt_callback);

    while (1)
    {
        app_evt_msg_t msg;

        ret = rtos_pop_from_queue(&app_evt_info.queue, &msg, BEKEN_WAIT_FOREVER);

        if (ret == BK_OK)
        {
            switch (msg.event)
            {
                case APP_EVT_ASR_WAKEUP:	//hi armino
                    is_standby = 0;
                    LOGI("APP_EVT_ASR_WAKEUP\n");
                    lvgl_app_init();
                    stop_countdown();
                    if (!is_network_provisioning){
                        led_app_set(LED_OFF_GREEN,0);
                    }
                    break;
                case APP_EVT_ASR_STANDBY:	//byebye armino
                    is_standby = 1;
                    LOGI("APP_EVT_ASR_STANDBY\n");
                    led_app_set(LED_SLOW_BLINK_GREEN,LED_LAST_FOREVER);
                    lvgl_app_deinit();
                    start_countdown(countdown_ms);
                    break;

//-------------------network event start ------------------------------------------------------------------
/*
 * Network abnormal event:APP_EVT_NETWORK_PROVISIONING_FAIL/APP_EVT_RECONNECT_NETWORK_FAIL/APP_EVT_RTC_CONNECTION_LOST/APP_EVT_AGENT_OFFLINE
 * Network resotre event:APP_EVT_AGENT_JOINED
 * If network retore event APP_EVT_AGENT_JOINED comes, it means all of the network abnormal event can be stop
 */
                case APP_EVT_NETWORK_PROVISIONING:
                    stop_countdown();
                    start_countdown(COUNTDOWN_NETWORK_PROVISIONING);
                    LOGI("APP_EVT_NETWORK_PROVISIONING\n");
                    is_network_provisioning = 1;
                    led_app_set(LED_REG_GREEN_ALTERNATE,LED_LAST_FOREVER);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_NETWORK_PROVISION);
#endif
                    break;

                case APP_EVT_NETWORK_PROVISIONING_SUCCESS:
                    LOGI("APP_EVT_NETWORK_PROVISIONING_SUCCESS\n");
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_NETWORK_PROVISION_SUCCESS);
#endif

                    stop_countdown();
                    start_countdown(countdown_ms);

                    break;

                case APP_EVT_NETWORK_PROVISIONING_FAIL:
                    LOGI("APP_EVT_NETWORK_PROVISIONING_FAIL\n");
                    network_err = 1;
                    led_app_set(LED_OFF_GREEN,0);
                    led_app_set(LED_FAST_BLINK_RED,LED_LAST_FOREVER);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_NETWORK_PROVISION_FAIL);
#endif
                    break;

                case APP_EVT_RECONNECT_NETWORK:
                    LOGI("APP_EVT_RECONNECT_NETWORK\n");
                    led_app_set(LED_OFF_RED,0);
                    led_app_set(LED_FAST_BLINK_GREEN,LED_LAST_FOREVER);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RECONNECT_NETWORK);
#endif
                    break;

                case APP_EVT_RECONNECT_NETWORK_SUCCESS:
                    LOGI("APP_EVT_RECONNECT_NETWORK_SUCCESS\n");
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RECONNECT_NETWORK_SUCCESS);
#endif
                    break;

                case APP_EVT_RECONNECT_NETWORK_FAIL:
                    LOGI("APP_EVT_RECONNECT_NETWORK_FAIL\n");
                    network_err = 1;
                    led_app_set(LED_OFF_GREEN,0);
                    led_app_set(LED_FAST_BLINK_RED,LED_LAST_FOREVER);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RECONNECT_NETWORK_FAIL);
#endif
                    break;

                case APP_EVT_RTC_CONNECTION_LOST:
                    network_err = 1;
                    LOGI("APP_EVT_RTC_CONNECTION_LOST\n");
                    led_app_set(LED_OFF_GREEN,0);
                    led_app_set(LED_FAST_BLINK_RED,LED_LAST_FOREVER);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RTC_CONNECTION_LOST);
#endif
                    break;

                case APP_EVT_AGENT_JOINED:	//doesn't know whether restore from error
                    LOGI("APP_EVT_AGENT_JOINED network_err=%d\n", network_err);
                    is_network_provisioning = 0;
                    if(network_err)
                    {
                        led_app_set(LED_OFF_RED,0);
                        LOGI("is_standby is =%d\n", is_standby);
                        if(is_standby)
                        {

                            led_app_set(LED_SLOW_BLINK_GREEN,LED_LAST_FOREVER);

                        }
                        network_err = 0;

                    }
                    else
                    {
                        if(!is_standby)
                        {
                            led_app_set(LED_REG_GREEN_ALTERNATE_OFF,0);

                        }else{
                            led_app_set(LED_SLOW_BLINK_GREEN, LED_LAST_FOREVER);
                        }
                        //led_app_set(LED_REG_GREEN_ALTERNATE_OFF, 0);
                    }
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_AGENT_JOINED);
#endif
                    break;
                case APP_EVT_AGENT_OFFLINE:
                    network_err = 1;
                    LOGI("APP_EVT_AGENT_OFFLINE\n");
                    led_app_set(LED_OFF_GREEN,0);
                    led_app_set(LED_FAST_BLINK_RED,LED_LAST_FOREVER);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_AGENT_OFFLINE);
#endif
                    break;

//-------------------network event end ------------------------------------------------------------------////


                case APP_EVT_LOW_VOLTAGE:
                    is_low_voltage_warning = 1;
                    LOGI("APP_EVT_LOW_VOLTAGE\n");
                    low_voltage_warning_tick = rtos_get_time();
                    led_app_set(LED_SLOW_BLINK_RED,LOW_VOLTAGE_BLINK_TIME);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_LOW_VOLTAGE);
#endif
                    break;

                case APP_EVT_CHARGING:
                    if((rtos_get_time() - low_voltage_warning_tick <= LOW_VOLTAGE_BLINK_TIME) && is_low_voltage_warning)
                        led_app_set(LED_OFF_RED,0);

                    low_voltage_warning_tick = 0;
                    is_low_voltage_warning = 0;
                    break;

                case APP_EVT_CLOSE_BLUETOOTH:
                    LOGI("APP_EVT_CLOSE_BLUETOOTH\n");
                    bk_genie_boarding_deinit();
                    bk_bluetooth_deinit();
                    break;
                case APP_EVT_POWER_ON:
                    led_app_set(LED_ON_GREEN,LED_LAST_FOREVER);
                    break;
                default:
                    break;
            }
        }
    }

    LOGI("%s, exit\r\n", __func__);
    rtos_delete_thread(NULL);
}



void app_event_init(void)
{
    int ret = BK_FAIL;

    os_memset(&app_evt_info, 0, sizeof(app_evt_info_t));

    ret = rtos_init_queue(&app_evt_info.queue,
                          "ae_queue",
                          sizeof(app_evt_msg_t),
                          15);

    if (ret != BK_OK)
    {
        LOGE("%s, init queue failed\r\n", __func__);
        return;
    }

    ret = rtos_create_thread(&app_evt_info.thread,
                             BEKEN_DEFAULT_WORKER_PRIORITY - 1,
                             "ae_thread",
                             (beken_thread_function_t)app_event_thread,
                             1024 * 4,
                             NULL);

    if (ret != BK_OK)
    {
        LOGE("%s, init thread failed\r\n", __func__);
        return;
    }
}
