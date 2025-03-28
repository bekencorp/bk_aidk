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
#include <modules/pm.h>

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

//red:if high priority conflicts with low-priority, should stay at high priority states
enum {
	WARNING_PROVIOSION_FAIL,	//LED_FAST_BLINK_RED
	WARNING_WIFI_FAIL,		//LED_FAST_BLINK_RED
	WARNING_RTC_CONNECT_LOST,	//LED_FAST_BLINK_RED
	WARNING_AGENT_OFFLINE,	//LED_FAST_BLINK_RED

	WARNING_LOW_BATTERY,	//LED_SLOW_BLINK_RED
}warning_t;
#define HIGH_PRIORITY_WARNING_MASK ((1<<WARNING_PROVIOSION_FAIL) | (1<<WARNING_WIFI_FAIL) | (1<<WARNING_RTC_CONNECT_LOST) | (1<<WARNING_AGENT_OFFLINE))
#define LOW_PRIORITY_WARNING_MASK ((1<<WARNING_LOW_BATTERY))
#define AI_RTC_CONNECT_LOST_FAIL   (1<<WARNING_RTC_CONNECT_LOST)
#define AI_AGENT_OFFLINE_FAIL      (1<<WARNING_AGENT_OFFLINE)
//green led, or red/green alternate led
enum {
	INDICATES_WIFI_RECONNECT,	//LED_FAST_BLINK_GREEN
	INDICATES_PROVISIONING,		//LED_REG_GREEN_ALTERNATE
	INDICATES_POWER_ON,  		//LED_ON_GREEN  default at power-on state, so green is always on
	INDICATES_STANDBY,			//LED_SLOW_BLINK_GREEN
	INDICATES_AGENT_CONNECT,	//LED_SLOW_BLINK_GREEN if at standby states, else clear it
}indicates_t;

static void led_blink(uint32_t* warning_state, uint32_t indicates_state)
{
	static uint32_t last_warning_state = 0;
	static uint32_t last_indicates_state = (1<<INDICATES_POWER_ON);

    //indicates
    if(indicates_state & (1<<INDICATES_PROVISIONING))
    {
        led_app_set(LED_REG_GREEN_ALTERNATE, LED_LAST_FOREVER);
    }else if (HIGH_PRIORITY_WARNING_MASK & (*warning_state))
    {
        led_app_set(LED_OFF_GREEN, 0);
    }else{
        if(indicates_state & (1<<INDICATES_WIFI_RECONNECT))
		{
			led_app_set(LED_FAST_BLINK_GREEN, LED_LAST_FOREVER);
		}else if(indicates_state & ((1<<INDICATES_STANDBY) | (1<<INDICATES_AGENT_CONNECT)))
		{
			led_app_set(LED_SLOW_BLINK_GREEN, LED_LAST_FOREVER);
		}else
        {
        led_app_set(LED_OFF_GREEN, 0);
        }
    }

    //warning
    if (indicates_state & (1<<INDICATES_PROVISIONING))
    {
        ;
    }else if(HIGH_PRIORITY_WARNING_MASK & (*warning_state)){
        led_app_set(LED_FAST_BLINK_RED, LED_LAST_FOREVER);
    }else if(LOW_PRIORITY_WARNING_MASK & (*warning_state)){
        led_app_set(LED_SLOW_BLINK_RED, LOW_VOLTAGE_BLINK_TIME);
        *warning_state = *warning_state & ~(1<<WARNING_LOW_BATTERY);
    }else{
        led_app_set(LED_OFF_RED, 0);
    }

    if(indicates_state != last_indicates_state)
	{
		LOGI("indicate=%d,last_indicat=%d,warning_state=%d\r\n", indicates_state, last_indicates_state, *warning_state);
		last_indicates_state = indicates_state;
	}

	if(*warning_state != last_warning_state)
	{
		LOGI("warning=%d,last_warning=%d,indicate=%d\r\n", *warning_state, last_warning_state, indicates_state);
		last_warning_state = *warning_state;
	}

}

static void app_event_thread(beken_thread_arg_t data)
{
	int ret = BK_OK;

    uint32_t is_standby = 1;

    uint32_t warning_state = 0;
	uint32_t indicates_state = (1<<INDICATES_POWER_ON);

    // uint32_t network_err = 0;
    uint32_t is_network_provisioning = 0;

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
                    indicates_state &= ~((1<<INDICATES_STANDBY) | (1<<INDICATES_AGENT_CONNECT));
                    LOGI("APP_EVT_ASR_WAKEUP\n");
                    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);
                    bk_wifi_sta_pm_disable();
                    //lvgl_app_init();
                    stop_countdown();
                    if (!is_network_provisioning){
                        led_app_set(LED_OFF_GREEN,0);
                    }
                    break;
                case APP_EVT_ASR_STANDBY:	//byebye armino
                    is_standby = 1;
                    indicates_state |= (1<<INDICATES_STANDBY);
                    indicates_state &= ~(1<<INDICATES_POWER_ON);
                    LOGI("APP_EVT_ASR_STANDBY\n");
                    //lvgl_app_deinit();
                    bk_wifi_sta_pm_enable();
                    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_240M);
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
                    indicates_state |= (1<<INDICATES_PROVISIONING);
                    indicates_state &= ~(1<<INDICATES_POWER_ON);
                    warning_state &= ~(1<<WARNING_PROVIOSION_FAIL);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_NETWORK_PROVISION);
#endif
                    break;

                case APP_EVT_NETWORK_PROVISIONING_SUCCESS:
                    indicates_state &= ~(1<<INDICATES_PROVISIONING);
                    warning_state &= ~(1<<WARNING_PROVIOSION_FAIL);
                    LOGI("APP_EVT_NETWORK_PROVISIONING_SUCCESS\n");
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_NETWORK_PROVISION_SUCCESS);
#endif

                    stop_countdown();
                    if(is_standby)
                        start_countdown(countdown_ms);

                    break;

                case APP_EVT_NETWORK_PROVISIONING_FAIL:
                    LOGI("APP_EVT_NETWORK_PROVISIONING_FAIL\n");
                    // network_err = 1;
                    indicates_state &= ~(1<<INDICATES_PROVISIONING);
                    warning_state |= 1<<WARNING_PROVIOSION_FAIL;

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_NETWORK_PROVISION_FAIL);
#endif
                    break;

                case APP_EVT_RECONNECT_NETWORK:
                    LOGI("APP_EVT_RECONNECT_NETWORK\n");
                    warning_state &= ~(1<<WARNING_WIFI_FAIL);
                    indicates_state |= (1<<INDICATES_WIFI_RECONNECT);
                    indicates_state &= ~(1<<INDICATES_POWER_ON);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RECONNECT_NETWORK);
#endif
                    break;

                case APP_EVT_RECONNECT_NETWORK_SUCCESS:
					warning_state &= ~(1<<WARNING_WIFI_FAIL);
                    indicates_state &= ~(1<<INDICATES_WIFI_RECONNECT);
                    if ((warning_state & AI_RTC_CONNECT_LOST_FAIL) == 0 && (warning_state & AI_AGENT_OFFLINE_FAIL) == 0)
                    {
                        if (is_standby)
                        {
                            indicates_state |= (1<<INDICATES_STANDBY);
                        }
                    }
                    LOGI("APP_EVT_RECONNECT_NETWORK_SUCCESS\n");
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RECONNECT_NETWORK_SUCCESS);
#endif
                    break;

                case APP_EVT_RECONNECT_NETWORK_FAIL:
                    LOGI("APP_EVT_RECONNECT_NETWORK_FAIL\n");
                    // network_err = 1;
                    warning_state |= 1<<WARNING_WIFI_FAIL;
                    indicates_state &= ~(1<<INDICATES_WIFI_RECONNECT);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RECONNECT_NETWORK_FAIL);
#endif
                    break;

                case APP_EVT_RTC_CONNECTION_LOST:
                    // network_err = 1;
                    LOGI("APP_EVT_RTC_CONNECTION_LOST\n");
                    warning_state |= 1<<WARNING_RTC_CONNECT_LOST;
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RTC_CONNECTION_LOST);
#endif
                    break;

                case APP_EVT_AGENT_JOINED:	//doesn't know whether restore from error
                    //indicates_state |= 1<<INDICATES_AGENT_CONNECT;
                    indicates_state &= ~(1<<INDICATES_POWER_ON);
                    warning_state &= ~((1<<WARNING_RTC_CONNECT_LOST) | (1<<WARNING_AGENT_OFFLINE) | (1<<WARNING_WIFI_FAIL));
                    LOGI("APP_EVT_AGENT_JOINED \n");
                    is_network_provisioning = 0;
                    indicates_state &= ~(1<<INDICATES_PROVISIONING);
                    if(is_standby)  //mie
                    {
                        indicates_state |= (1<<INDICATES_STANDBY);
                    }
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_AGENT_JOINED);
#endif
                    break;
                case APP_EVT_AGENT_OFFLINE:
                    // network_err = 1;
                    LOGI("APP_EVT_AGENT_OFFLINE\n");
                    indicates_state &= ~(1<<INDICATES_AGENT_CONNECT);
                    warning_state |= 1<<WARNING_AGENT_OFFLINE;
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_AGENT_OFFLINE);
#endif
                    break;

//-------------------network event end ------------------------------------------------------------------////


                case APP_EVT_LOW_VOLTAGE:
                    LOGI("APP_EVT_LOW_VOLTAGE\n");
                    warning_state |= 1<<WARNING_LOW_BATTERY;
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_LOW_VOLTAGE);
#endif
                    break;

                case APP_EVT_CHARGING:
                    LOGI("APP_EVT_CHARGING\n");
					warning_state &= ~(1<<WARNING_LOW_BATTERY);
                    break;

                case APP_EVT_CLOSE_BLUETOOTH:
                    LOGI("APP_EVT_CLOSE_BLUETOOTH\n");
                    bk_genie_boarding_deinit();
#if CONFIG_NET_PAN && !(CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    bk_bluetooth_deinit();
#endif
                    break;

                default:
                    break;
            }

			//led blink by states
            led_blink(&warning_state, indicates_state);
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
