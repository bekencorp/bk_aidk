#include <common/sys_config.h>
#include <components/log.h>
#include <string.h>

#include <key_app_service.h>
#include <key_app_config.h>
#include <driver/gpio.h>
#include "gpio_driver.h"
#if (CONFIG_SYS_CPU0)
#include "audio_config.h"
#include "aud_intf.h"
#include "bk_factory_config.h"
#if CONFIG_BK_SMART_CONFIG
#include "bk_genie_comm.h"
#include "bk_smart_config.h"
#endif
#endif
#if (CONFIG_USR_KEY_CFG_EN)
#include "usr_key_cfg.h"
#endif

#include "app_event.h"
#include <common/bk_include.h>
#include "components/bluetooth/bk_dm_bluetooth.h"
#include "app_main.h"
#if (CONFIG_SYS_CPU0 && (CONFIG_BK_WSS_TRANS || CONFIG_BK_WSS_TRANS_NOPSRAM))
#include "bk_wss/bk_wss.h"
#endif


#define TAG "key_service"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#if (CONFIG_SYS_CPU0)
uint32_t volume = 7;   // volume level, not gain.
uint32_t g_volume_gain[SPK_VOLUME_LEVEL] = {0};


static void volume_increase(void)
{
    BK_LOGI(TAG, " volume up\r\n");

    if (volume == (SPK_VOLUME_LEVEL-1))
    {
        BK_LOGI(TAG, "volume have reached maximum volume: %d\n", SPK_GAIN_MAX);
        return;
    }

    if (BK_OK == bk_aud_intf_set_spk_gain(g_volume_gain[volume+1]))
    {
        volume += 1;
        if (0 != bk_config_write("volume", (void *)&volume, 4))
        {
            BK_LOGE(TAG, "storage volume: %d fail\n", volume);
        }
        BK_LOGI(TAG, "current volume: %d\n", volume);
    }
    else
    {
        BK_LOGI(TAG, "set volume fail\n");
    }
}

static void volume_decrease(void)
{
    BK_LOGI(TAG, " volume down\r\n");

    if (volume == 0)
    {
        BK_LOGI(TAG, "volume have reached minimum volume: 0\n");
        return;
    }

    if (BK_OK == bk_aud_intf_set_spk_gain(g_volume_gain[volume-1]))
    {
        volume -= 1;
        if (0 != bk_config_write("volume", (void *)&volume, 4))
        {
            BK_LOGE(TAG, "storage volume: %d fail\n", volume);
        }
        BK_LOGI(TAG, "current volume: %d\n", volume);
    }
    else
    {
        BK_LOGI(TAG, "set volume fail\n");
    }
}

static void power_off(void)
{
    BK_LOGI(TAG, " power_off\r\n");
    BK_LOGW(TAG, " ************TODO:Just force deep sleep for Demo!\r\n");
    //extern bk_err_t audio_turn_off(void);
    //extern bk_err_t video_turn_off(void);
    //audio_turn_off();
    //video_turn_off();
    bk_reboot_ex(RESET_SOURCE_FORCE_DEEPSLEEP);
}

static void power_on(void)
{
    BK_LOGI(TAG, "power_on\r\n");
}

static void ai_agent_config(void)
{
    //BK_LOGW(TAG, " ************TODO:AI Agent doesn't complete!\r\n");
}

static KeyConfig_t key_config[] = KEY_DEFAULT_CONFIG_TABLE;

/*Do not execute blocking or time-consuming long code in event handler
 functions. The reason is that key_thread processes messages in a
 single task in sequence. If a handler function blocks or takes too
 long to execute, it will cause subsequent key events to be responded to untimely.*/
static void handle_system_event(key_event_t event)
{
    uint32_t time;

    extern void bk_bt_app_avrcp_ct_vol_change(uint32_t platform_vol);
    switch (event)
    {
        case VOLUME_UP:
        {
            uint32_t old_volume = volume;
            volume_increase();
            if(old_volume != volume)
            {
#if CONFIG_A2DP_SINK_DEMO
                bk_bt_app_avrcp_ct_vol_change(volume);
#endif
            }
        }
            break;
        case VOLUME_DOWN:
        {
            uint32_t old_volume = volume;
            volume_decrease();
            if(old_volume != volume)
            {
#if CONFIG_A2DP_SINK_DEMO
                bk_bt_app_avrcp_ct_vol_change(volume);
#endif
            }
        }
            break;
        case SHUT_DOWN:
            time = rtos_get_time(); //long press more than 6s
            if (time < 9000)
            {
                break;
            }
            power_off();
            break;
        case POWER_ON:
            power_on();
            break;
        case AI_AGENT_CONFIG:
            ai_agent_config();
            break;
        case CONFIG_NETWORK:
            BK_LOGW(TAG, "Start to config network!\n");
            bk_sconf_start_to_config_network();
            break;
        case IR_MODE_SWITCH:	////image recognition mode switch
            app_event_send_msg(APP_EVT_IR_MODE_SWITCH, 0);
            break;
        case FACTORY_RESET:
            BK_LOGW(TAG, "trigger factory config reset\r\n");
            //bk_bluetooth_deinit(); //no need close bluetooth
            bk_factory_reset();
            bk_reboot();
            break;
#if CONFIG_BK_WSS_TRANS_NOPSRAM
        case AUDIO_BUF_APPEND:
            BK_LOGW(TAG, "audio append start!\n");
            app_event_send_msg(APP_EVT_ASR_WAKEUP, 0);
            bk_wss_state_event(WSS_EVENT_RECORDING_START, NULL);
            break;
#endif
        // 其他事件处理...
        default:
            break;
    }
}

void volume_init(void)
{
    int volume_size = bk_config_read("volume", (void *)&volume, 4);
    if (volume_size != 4)
    {
        BK_LOGE(TAG, "read volume config fail, use default config volume_size:%d\n", volume_size);
    }

    if (volume > (SPK_VOLUME_LEVEL-1)) {
        volume = SPK_VOLUME_LEVEL-1;
        if (0 != bk_config_write("volume", (void *)&volume, 4))
        {
            BK_LOGE(TAG, "storage volume: %d fail\n", volume);
        }
    }

    /* SPK_GAIN_MAX * [(exp(i/(SPK_VOLUME_LEVEL-1)-1)/(exp(1)-1)] */
    uint32_t step[SPK_VOLUME_LEVEL] = {0,6,12,20,28,37,47,58,71,84,100};
    for (uint32_t i = 0; i < SPK_VOLUME_LEVEL; i++) {
        g_volume_gain[i] = SPK_GAIN_MAX * step[i]/100;
    }
}

void bk_key_register_wakeup_source(void)
{
    for (uint8_t i = 0; i < sizeof(key_config) / sizeof(KeyConfig_t); i++)
    {
        if ((key_config[i].short_event == POWER_ON) || (key_config[i].double_event == POWER_ON) || (key_config[i].long_event == POWER_ON))
        {
            if (key_config[i].active_level == LOW_LEVEL_TRIGGER)
            {
                bk_gpio_register_wakeup_source(key_config[i].gpio_id, GPIO_INT_TYPE_FALLING_EDGE);
            }
            else
            {
                bk_gpio_register_wakeup_source(key_config[i].gpio_id, GPIO_INT_TYPE_RISING_EDGE);
            }
        }
    }
}

uint32_t volume_get_current()
{
    return volume;
}

uint32_t volume_get_level_count()
{
    return SPK_VOLUME_LEVEL;
}

void volume_set_abs(uint8_t level, uint8_t has_precision)
{
#define PRECISION_GUARANTEE_GAIN 2
    bk_err_t ret = 0;

    BK_LOGI(TAG, "%s volume abs %d %d\n", __func__, level, has_precision);

    if (level > SPK_VOLUME_LEVEL - 1)
    {
        BK_LOGE(TAG, "%s invalid level %d >= %d\n", __func__, level, SPK_VOLUME_LEVEL);
        level = SPK_VOLUME_LEVEL - 1;
    }

    if(level == 0 && has_precision)
    {
        uint8_t final = (PRECISION_GUARANTEE_GAIN < g_volume_gain[1] ? PRECISION_GUARANTEE_GAIN: g_volume_gain[1]);
        BK_LOGW(TAG, "%s set raw gain %d because precision\n", __func__, final);
        ret = bk_aud_intf_set_spk_gain(final);
    }
    else
    {
        ret = bk_aud_intf_set_spk_gain(g_volume_gain[level]);
    }

    if (BK_OK == ret)
    {
        volume = level;

        if (0 != bk_config_write("volume", (void *)&volume, sizeof(volume)))
        {
            BK_LOGE(TAG, "%s storage volume: %d fail\n", __func__, level);
        }

        BK_LOGI(TAG, "%s current volume: %d\n", __func__, level);
    }
    else
    {
        BK_LOGE(TAG, "%s set volume %d fail\n", __func__, level);
    }
}

void bk_key_service_init(void)
{
    register_event_handler(handle_system_event);
    bk_key_driver_init(key_config, sizeof(key_config) / sizeof(KeyConfig_t));
}


#endif
