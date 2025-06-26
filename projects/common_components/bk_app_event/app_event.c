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
#include "led_app.h"
#include "countdown_app.h"
#include "components/bluetooth/bk_dm_bluetooth.h"
#include "boarding_service.h"
#include "bk_factory_config.h"
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#include "aud_intf.h"
#include "aud_intf_types.h"
#endif

#include "bk_smart_config.h"

#include "bat_monitor.h"
#include "bk_ota_private.h"

#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
#include "app_audio_arbiter.h"
#endif
#if (CONFIG_SYS_CPU0 && CONFIG_LINGXIN_AI_EN)
#include "voice_chat_machine.h"
#endif

#define TAG "app_evt"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static app_event_handler_t *s_event_handlers = NULL;
static beken_mutex_t s_event_mutex = NULL;

#if (CONFIG_DUAL_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_FONT_DISPLAY)
extern void lvgl_app_init(void);
extern void lvgl_app_deinit(void);
extern uint8_t lvgl_app_init_flag;
#endif
extern bk_err_t agora_stop(void);
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

static uint8_t battery_event_callback(evt_battery event_param)
{
    switch(event_param)
    {
        case EVT_BATTERY_CHARGING:
            app_event_send_msg(APP_EVT_CHARGING, 0);
            break;
        case EVT_BATTERY_LOW_VOLTAGE:
            app_event_send_msg(APP_EVT_LOW_VOLTAGE, 0);
            break;
        case EVT_SHUTDOWN_LOW_BATTERY:
            app_event_send_msg(APP_EVT_SHUTDOWN_LOW_BATTERY, 0);
            break;
        default :
            break;
    }
    return 0;
}

#if CONFIG_OTA_DISPLAY_PICTURE_DEMO
extern bk_err_t audio_turn_on(void);
bk_err_t bk_ota_reponse_state_to_audio(int ota_state)
{
	int ret = BK_FAIL;
	LOGI("%s ota_state :%d\n", __func__,ota_state);
	if(audio_turn_on() != BK_OK)
	{
		LOGE("%s audion turn off :%d\n", __func__);
		return ret;
	}

	switch (ota_state)
	{
		case APP_EVT_OTA_SUCCESS : //success
			#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
			ret = bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_OTA_UPDATE_SUCCESS);
			#endif
		break;
		case APP_EVT_OTA_FAIL : //fail
			#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
			ret = bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_OTA_UPDATE_FAIL);
			#endif
		break;
		default:
			break;
	}
	LOGI("%s complete %x\n", __func__, ret);
	return ret;
}
#endif


static void app_event_thread(beken_thread_arg_t data)
{
	int ret = BK_OK;

    uint32_t is_standby = 1;
    uint32_t is_joined_agent = 0;

    uint32_t warning_state = 0;
	  uint32_t indicates_state = (1<<INDICATES_POWER_ON);

    // uint32_t network_err = 0;
    uint32_t is_network_provisioning = 0;

    uint32_t s_active_tickets = (1 << COUNTDOWN_TICKET_STANDBY);
    ota_event_callback_register(ota_event_callback);
#if CONFIG_COUNTDOWN
    update_countdown(s_active_tickets);
#endif
#if CONFIG_BAT_MONITOR
    battery_event_callback_register(battery_event_callback);
#endif
    media_app_asr_evt_register_callback(app_event_asr_evt_callback);

    while (1)
    {
        app_evt_msg_t msg;

        ret = rtos_pop_from_queue(&app_evt_info.queue, &msg, BEKEN_WAIT_FOREVER);

        if (ret == BK_OK)
        {
            bool skip_countdown_update = false;
            switch (msg.event)
            {
#if CONFIG_BK_SMART_CONFIG
                case APP_EVT_IR_MODE_SWITCH:
                    if (is_standby == 0) {
                        bk_sconf_begin_to_switch_ir_mode();
                    }
                    break;
#endif
                case APP_EVT_ASR_WAKEUP:	//hi armino
                    is_standby = 0;
                    indicates_state &= ~(1<<INDICATES_STANDBY);
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_STANDBY);
                    LOGI("APP_EVT_ASR_WAKEUP\n");
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_ASR_WAKEUP);
#endif
                    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);
                    bk_wifi_sta_pm_disable();
#if (CONFIG_DUAL_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_FONT_DISPLAY)
                    lvgl_app_init();
#endif
                    //TODO optimize
                    if (!is_network_provisioning){
                        led_app_set(LED_OFF_GREEN,0);
                    }
#if (CONFIG_SYS_CPU0 && CONFIG_LINGXIN_AI_EN)
					os_printf("%s line:%d State_Event_Wakeup_Detected\r\n", __func__, __LINE__);
					state_machine_run_event(State_Event_Wakeup_Detected);
#endif

                    break;
                case APP_EVT_ASR_STANDBY:	//byebye armino
                    is_standby = 1;
                    indicates_state |= (1<<INDICATES_STANDBY);
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_STANDBY);
                    LOGI("APP_EVT_ASR_STANDBY\n");
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE && !(CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_ASR_STANDBY);
#endif
#if (CONFIG_DUAL_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_FONT_DISPLAY)
                    lvgl_app_deinit();
#endif
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
                    indicates_state &= ~((1<<INDICATES_AGENT_CONNECT) | (1<<INDICATES_POWER_ON) | (1<<INDICATES_WIFI_RECONNECT));
                    warning_state &= ~ (HIGH_PRIORITY_WARNING_MASK);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_NETWORK_PROVISION);
#endif
#if (CONFIG_DUAL_SCREEN_AVI_PLAY)
                    media_app_lvgl_switch_ui(LVGL_UI_DISP_IN_TEXT);
#endif
                    break;

                case APP_EVT_NETWORK_PROVISIONING_SUCCESS:
                    indicates_state &= ~(1<<INDICATES_PROVISIONING);
                    indicates_state |= (1<<INDICATES_AGENT_CONNECT);
                    warning_state &= ~(1<<WARNING_PROVIOSION_FAIL);
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_PROVISIONING);
                    // s_active_tickets |= (1 << COUNTDOWN_TICKET_STANDBY);
                    LOGI("APP_EVT_NETWORK_PROVISIONING_SUCCESS\n");
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
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
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_NETWORK_PROVISION_FAIL);
#endif
                    break;

                case APP_EVT_RECONNECT_NETWORK:
                    LOGI("APP_EVT_RECONNECT_NETWORK\n");
                    warning_state &= ~(1<<WARNING_WIFI_FAIL);
                    indicates_state |= (1<<INDICATES_WIFI_RECONNECT);
                    indicates_state &= ~(1<<INDICATES_POWER_ON);
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RECONNECT_NETWORK);
#endif
                    break;

                case APP_EVT_RECONNECT_NETWORK_SUCCESS:
                    LOGI("APP_EVT_RECONNECT_NETWORK_SUCCESS\n");
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_PROVISIONING);
                    if (warning_state & WIFI_FAIL)
                    {
                        if (is_joined_agent && is_standby)
                        {
                            indicates_state |= (1<<INDICATES_STANDBY);
                        }

                    }else{
                        indicates_state |= (1<<INDICATES_AGENT_CONNECT);
                    }

					          warning_state &= ~(1<<WARNING_WIFI_FAIL);
                    indicates_state &= ~(1<<INDICATES_WIFI_RECONNECT);

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
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
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RECONNECT_NETWORK_FAIL);
#endif
                    break;

                case APP_EVT_RTC_CONNECTION_LOST:
                    // network_err = 1;
                    LOGI("APP_EVT_RTC_CONNECTION_LOST\n");
                    warning_state |= 1<<WARNING_RTC_CONNECT_LOST;
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_RTC_CONNECTION_LOST);
#endif
                    break;


                case APP_EVT_RTC_REJOIN_SUCCESS:
                    LOGI("APP_EVT_RTC_REJOIN_SUCCESS\n");
                    warning_state &= ~(1<<WARNING_RTC_CONNECT_LOST);
                    if (is_joined_agent && is_standby)
                    {
                        indicates_state |= (1<<INDICATES_STANDBY);
                    }
                    break;

                case APP_EVT_AGENT_JOINED:	//doesn't know whether restore from error
                    is_joined_agent = 1;
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_NETWORK_ERROR);
                    indicates_state &= ~(1<<INDICATES_AGENT_CONNECT);
                    warning_state &= ~((1<<WARNING_RTC_CONNECT_LOST) | (1<<WARNING_AGENT_OFFLINE) | (1<<WARNING_WIFI_FAIL) | 1<<WARNING_AGENT_AGENT_START_FAIL);
                    LOGI("APP_EVT_AGENT_JOINED \n");
                    //TODO optimize
                    is_network_provisioning = 0;
                    indicates_state &= ~(1<<INDICATES_PROVISIONING);
                    if(is_standby)  //mie
                    {
                        indicates_state |= (1<<INDICATES_STANDBY);
                    }
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_AGENT_JOINED);
#endif
                    break;
                case APP_EVT_AGENT_OFFLINE:
                    // network_err = 1;
                    is_joined_agent = 0;
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_NETWORK_ERROR);
                    LOGI("APP_EVT_AGENT_OFFLINE\n");
                    warning_state |= 1<<WARNING_AGENT_OFFLINE;
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_AGENT_OFFLINE);
#endif
                    break;

                case APP_EVT_AGENT_START_FAIL:
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_NETWORK_ERROR);
                    LOGI("APP_EVT_AGENT_START_FAIL\n");
                    warning_state |= 1<<WARNING_AGENT_AGENT_START_FAIL;
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_AGENT_START_FAIL);
#endif
                    break;

                case APP_EVT_AGENT_DEVICE_REMOVE:
                    LOGI("APP_EVT_AGENT_DEVICE_REMOVE\n");
#if CONFIG_AGORA_IOT_SDK
                    agora_stop();
#endif
                    break;

//-------------------network event end ------------------------------------------------------------------////


                case APP_EVT_LOW_VOLTAGE:
                    LOGI("APP_EVT_LOW_VOLTAGE\n");
                    skip_countdown_update = true;
                    warning_state |= 1<<WARNING_LOW_BATTERY;
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
                    if(app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_AI ||
                                    app_audio_arbiter_get_current_play_entry() == AUDIO_SOURCE_ENTRY_END)
#endif
                    bk_aud_intf_voc_play_prompt_tone(AUD_INTF_VOC_LOW_VOLTAGE);
#endif
                    break;

                case APP_EVT_CHARGING:
                    LOGI("APP_EVT_CHARGING\n");
                    skip_countdown_update = true;
					          warning_state &= ~(1<<WARNING_LOW_BATTERY);
                    break;

                case APP_EVT_SHUTDOWN_LOW_BATTERY:
                    LOGI("APP_EVT_SHUTDOWN_LOW_BATTERY\n");
                    skip_countdown_update = true;
                    bk_config_sync_flash();
                    break;

                case APP_EVT_CLOSE_BLUETOOTH:
#if CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO
                    LOGI("APP_EVT_CLOSE_BLUETOOTH no need to close !!!\n");
#else
                    LOGI("APP_EVT_CLOSE_BLUETOOTH\n");
#if CONFIG_BK_BOARDING_SERVICE
                    bk_genie_boarding_deinit();
#endif
#if CONFIG_BLUETOOTH
                    bk_bluetooth_deinit();
#endif
#endif
                    break;

                // OTA相关事件
                case APP_EVT_OTA_START:
                    LOGI("APP_EVT_OTA_START\n");
                    s_active_tickets |= (1 << COUNTDOWN_TICKET_OTA);
                    break;

                case APP_EVT_OTA_SUCCESS:
                    LOGI("APP_EVT_OTA_SUCCESS\n");
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_OTA);
                    #if CONFIG_OTA_DISPLAY_PICTURE_DEMO
                    bk_ota_reponse_state_to_audio(msg.event);
                    #endif
                    break;

                case APP_EVT_OTA_FAIL:
                    LOGI("APP_EVT_OTA_FAIL\n");
                    s_active_tickets &= ~(1 << COUNTDOWN_TICKET_OTA);
                    #if CONFIG_OTA_DISPLAY_PICTURE_DEMO
                    bk_ota_reponse_state_to_audio(msg.event);
                    #endif
                    break;
                case APP_EVT_SYNC_FLASH:
                    LOGI("APP_EVT_SYNC_FLASH\n");
                    bk_sconf_sync_flash_safely();
                    break;
                default:
                    break;
            }
            if(!skip_countdown_update)
            {
#if CONFIG_COUNTDOWN
                update_countdown(s_active_tickets);
#endif
            }

#if CONFIG_LED_BLINK
			      //led blink by states
            led_blink(&warning_state, indicates_state);
#endif

            rtos_lock_mutex(&s_event_mutex);
            app_event_handler_t *handler = s_event_handlers;
            while (handler)
            {
                if (handler->event_type == msg.event)
                {
                  handler->callback(&msg, handler->user_data);
                }
                handler = handler->next;
            }
            rtos_unlock_mutex(&s_event_mutex);
        }
    }

    LOGI("%s, exit\r\n", __func__);
    rtos_delete_thread(NULL);
}

int app_event_register_handler(app_evt_type_t event_type,
                              app_event_callback_t callback,
                              void *user_data)
{
    app_event_handler_t *handler = os_malloc(sizeof(app_event_handler_t));
    if (!handler) return BK_ERR_NO_MEM;

    handler->event_type = event_type;
    handler->callback = callback;
    handler->user_data = user_data;
    handler->next = NULL;

    rtos_lock_mutex(&s_event_mutex);
    handler->next = s_event_handlers;
    s_event_handlers = handler;
    rtos_unlock_mutex(&s_event_mutex);

    return BK_OK;
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
                             1024 * 5,
                             NULL);

    if (ret != BK_OK)
    {
        LOGE("%s, init thread failed\r\n", __func__);
        return;
    }

    ret = rtos_init_mutex(&s_event_mutex);

    if (ret != BK_OK)
    {
        LOGE("%s, init s_event_mutex failed\r\n", __func__);
        return;
    }

}
