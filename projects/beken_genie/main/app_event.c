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
#include "bk_ota_private.h" 
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

static uint8_t ota_event_callback(evt_ota event_param)
{

    switch(event_param)
    {
        case EVT_OTA_START:
            app_event_send_msg(APP_EVT_OTA_START, 0);
            break;
        case EVT_OTA_FAIL:
            app_event_send_msg(APP_EVT_OTA_FAIL, 0);
            break;  
        case EVT_OTA_SUCCESS:
            app_event_send_msg(APP_EVT_OTA_SUCCESS, 0);
            break;
        default :
            break;
    }
    return 0;
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


#define COUNTDOWN_INFINITE 0xFFFFFFFF
/* 新增倒计时票源类型,按照优先级排序*/
typedef enum {
    COUNTDOWN_TICKET_PROVISIONING,   // 配网倒计时(5分钟), 最高优先级
    COUNTDOWN_TICKET_NETWORK_ERROR,  //网络错误倒计时(5分钟) ，中优先级
    COUNTDOWN_TICKET_STANDBY,        // 待机倒计时(3分钟)，低优先级
    COUNTDOWN_TICKET_OTA,      // OTA 事件，特殊逻辑，不参与比较
    COUNTDOWN_TICKET_MAX
} countdown_ticket_t;

/* 各票源对应的倒计时时长(毫秒) */
static const uint32_t s_ticket_durations[COUNTDOWN_TICKET_MAX] = {
    [COUNTDOWN_TICKET_PROVISIONING] = 5 * 60 * 1000,  // 5分钟
    [COUNTDOWN_TICKET_NETWORK_ERROR] = 5 * 60 * 1000,  // 5分钟
    [COUNTDOWN_TICKET_STANDBY]      = 3 * 60 * 1000,  // 3分钟
    [COUNTDOWN_TICKET_OTA]    = COUNTDOWN_INFINITE,              // 暂停倒计时
};

static uint32_t s_active_tickets = 0;  // 使用位掩码记录活跃票源

/* 更新倒计时状态 */
static void update_countdown()
{

    // 检查OTA暂停票（最高优先级）
    if(s_active_tickets & (1 << COUNTDOWN_TICKET_OTA)) {
        LOGI("ota event start, stop countdown\r\n");
        stop_countdown();
        return;
    }

    static countdown_ticket_t last_selected_ticket = COUNTDOWN_TICKET_MAX;
    countdown_ticket_t selected_ticket = COUNTDOWN_TICKET_MAX;


    for(int i = 0; i < COUNTDOWN_TICKET_MAX; i++) {
        if (i == COUNTDOWN_TICKET_OTA)
            continue;
        
        if(s_active_tickets & (1 << i))
        {
            selected_ticket = i;
            break;
        }
    }

    // 应用倒计时裁决结果
    if(selected_ticket != COUNTDOWN_TICKET_MAX) {
        const uint32_t max_duration = s_ticket_durations[selected_ticket];

        if (selected_ticket != last_selected_ticket)
        {
            const uint32_t duration = max_duration;
            start_countdown(duration);
            LOGI("selected_ticket is %d, last_selected_ticket is %d,duration is %d\r\n",selected_ticket , last_selected_ticket, duration);
            last_selected_ticket = selected_ticket;
        }
    } else {
        LOGI("selected_ticket is %d, COUNTDOWN_TICKET_MAX is %d\r\n",selected_ticket , COUNTDOWN_TICKET_MAX);
        LOGI("stop countdown\r\n");
        stop_countdown();
        last_selected_ticket = COUNTDOWN_TICKET_MAX;
    }

}

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

    s_active_tickets = (1 << COUNTDOWN_TICKET_STANDBY);
    ota_event_callback_register(ota_event_callback);
    update_countdown();

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
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_STANDBY);
                    LOGI("APP_EVT_ASR_WAKEUP\n");
                    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);
                    bk_wifi_sta_pm_disable();
                    lvgl_app_init();
                    if (!is_network_provisioning){
                        led_app_set(LED_OFF_GREEN,0);
                    }
                    break;
                case APP_EVT_ASR_STANDBY:	//byebye armino
                    is_standby = 1;
                    indicates_state |= (1<<INDICATES_STANDBY);
                    indicates_state &= ~(1<<INDICATES_POWER_ON);
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_STANDBY);
                    LOGI("APP_EVT_ASR_STANDBY\n");
                    lvgl_app_deinit();
                    bk_wifi_sta_pm_enable();
                    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_240M);
                    break;

//-------------------network event start ------------------------------------------------------------------
/*
 * Network abnormal event:APP_EVT_NETWORK_PROVISIONING_FAIL/APP_EVT_RECONNECT_NETWORK_FAIL/APP_EVT_RTC_CONNECTION_LOST/APP_EVT_AGENT_OFFLINE
 * Network resotre event:APP_EVT_AGENT_JOINED
 * If network retore event APP_EVT_AGENT_JOINED comes, it means all of the network abnormal event can be stop
 */
                case APP_EVT_NETWORK_PROVISIONING:
                    LOGI("APP_EVT_NETWORK_PROVISIONING\n");
                    is_network_provisioning = 1;
                    //优先级最高
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_NETWORK_ERROR);
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_PROVISIONING);
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
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_PROVISIONING);
                    // s_active_tickets |= (1 << COUNTDOWN_TICKET_STANDBY);
                    LOGI("APP_EVT_NETWORK_PROVISIONING_SUCCESS\n");
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_NETWORK_PROVISION_SUCCESS);
#endif
                    break;

                case APP_EVT_NETWORK_PROVISIONING_FAIL:
                    LOGI("APP_EVT_NETWORK_PROVISIONING_FAIL\n");
                    // network_err = 1;
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_PROVISIONING);
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_NETWORK_ERROR);
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
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_PROVISIONING);
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
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_NETWORK_ERROR);
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
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_NETWORK_ERROR);
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
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_NETWORK_ERROR);
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
                    bk_bluetooth_deinit();
                    break;

                // OTA相关事件
                case APP_EVT_OTA_START:
                    LOGI("APP_EVT_OTA_START\n");
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_OTA);
                    break;

                case APP_EVT_OTA_SUCCESS:
                    LOGI("APP_EVT_OTA_SUCCESS\n");
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_OTA);
                    break;
                
                case APP_EVT_OTA_FAIL:
                    LOGI("APP_EVT_OTA_FAIL\n");
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_OTA);
                    break;

                default:
                    break;
            }
            update_countdown();
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
