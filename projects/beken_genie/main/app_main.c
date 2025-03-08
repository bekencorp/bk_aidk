#include <common/sys_config.h>
#include <components/log.h>
#include <modules/wifi.h>
#include <components/netif.h>
#include <components/event.h>
#include <string.h>

#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>
#include "cli.h"
#include "media_service.h"
#include <driver/pwr_clk.h>
#include <driver/pwr_clk.h>
#include <modules/pm.h>
#if CONFIG_BUTTON
#include <key_main.h>
#include <key_adapter.h>
#endif
#include <driver/gpio.h>
#include "gpio_driver.h"
#include "bk_genie_comm.h"
#include "wifi_boarding_utils.h"
#if (CONFIG_SYS_CPU0)
#include "agora_config.h"
#include "aud_intf.h"
#if CONFIG_NETWORK_AUTO_RECONNECT
#include "bk_genie_smart_config.h"
#endif
#endif

#include "app_event.h"

#include <led_blink.h>
extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);
extern int bk_cli_init(void);
extern void bk_set_jtag_mode(uint32_t cpu_id, uint32_t group_id);

#define TAG "AGORA"

//#define AUTOCONNECT_WIFI

#define CONFIG_WIFI_SSID            "BEKEN-CES"//"test123"//"biubiu"//"MEGSCREEN_TEST"//"cs-ruowang-2.4G"//"Carl"//"NXIOT"
#define CONFIG_WIFI_PASSWORD        "1233211234567"//"1234567890"//"87654321"//"987654321"//"wohenruo"//"12345678"//"88888888"

#ifdef CONFIG_LDO3V3_ENABLE
#ifndef LDO3V3_CTRL_GPIO
#ifdef CONFIG_LDO3V3_CTRL_GPIO
#define LDO3V3_CTRL_GPIO    CONFIG_LDO3V3_CTRL_GPIO
#else
#define LDO3V3_CTRL_GPIO    GPIO_52
#endif
#endif
#endif

#if (CONFIG_SYS_CPU0)
static uint32_t volume = SPK_GAIN_MAX * 0.65;
#endif

#if (CONFIG_SYS_CPU0)
#define AGORA_RTC_CMD_CNT   (sizeof(s_agora_rtc_commands) / sizeof(struct cli_command))

extern void cli_agora_rtc_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
extern void cli_agora_rtc_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
extern void lvgl_app_init(void);

static const struct cli_command s_agora_rtc_commands[] =
{
    {"agora_test", "agora_test ...", cli_agora_rtc_test_cmd},
    {"agora_debug", "agora_debug ...", cli_agora_rtc_debug_cmd},
#if CONFIG_NETWORK_AUTO_RECONNECT
    {"bk_smart_config_erase", "bk_smart_config_erase", bk_genie_smart_config_cli}
#endif
};

static int agora_rtc_cli_init(void)
{
    return cli_register_commands(s_agora_rtc_commands, AGORA_RTC_CMD_CNT);
}

#elif CONFIG_SYS_CPU1
#define AGORA_RTC_CMD_CNT   (sizeof(s_agora_rtc_commands) / sizeof(struct cli_command))
extern void cli_agora_rtc_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
static const struct cli_command s_agora_rtc_commands[] =
{
    {"agora_debug", "agora_debug ...", cli_agora_rtc_debug_cmd},
};

static int agora_rtc_cli_init(void)
{
    return cli_register_commands(s_agora_rtc_commands, AGORA_RTC_CMD_CNT);
}

#endif

#if (CONFIG_SYS_CPU0)
// 按键 1 的回调函数
void volume_increase()
{
    BK_LOGI(TAG, " volume up\r\n");
    if (volume == SPK_GAIN_MAX)
    {
        BK_LOGI(TAG, "volume have reached maximum volume: %d\n", SPK_GAIN_MAX);
        return;
    }
    if (BK_OK == bk_aud_intf_set_spk_gain(volume + 1))
    {
        volume += 1;
        BK_LOGI(TAG, "current volume: %d\n", volume);
    }
    else
    {
        BK_LOGI(TAG, "set volume fail\n");
    }
}

void volume_decrease()
{
    BK_LOGI(TAG, " volume down\r\n");
    if (volume == 0)
    {
        BK_LOGI(TAG, "volume have reached minimum volume: 0\n");
        return;
    }
    if (BK_OK == bk_aud_intf_set_spk_gain(volume - 1))
    {
        volume -= 1;
        BK_LOGI(TAG, "current volume: %d\n", volume);
    }
    else
    {
        BK_LOGI(TAG, "set volume fail\n");
    }
}

void power_off()
{
    BK_LOGI(TAG, " power_off\r\n");


    BK_LOGW(TAG, " ************TODO:Just force deep sleep for Demo!\r\n");
    bk_reboot_ex(RESET_SOURCE_FORCE_DEEPSLEEP);
}

void power_on()
{
    BK_LOGI(TAG, "power_on\r\n");
}

void ai_agent_config()
{
    //BK_LOGW(TAG, " ************TODO:AI Agent doesn't complete!\r\n");
}

/*Do not execute blocking or time-consuming long code in event handler
 functions. The reason is that key_thread processes messages in a
 single task in sequence. If a handler function blocks or takes too
 long to execute, it will cause subsequent key events to be responded to untimely.*/
static void handle_system_event(key_event_t event)
{
    switch (event)
    {
        case VOLUME_UP:
            volume_increase();
            break;
        case VOLUME_DOWN:
            volume_decrease();
            break;
        case SHUT_DOWN:
            power_off();
            break;
        case POWER_ON:
            power_on();
            break;
        case AI_AGENT_CONFIG:
            ai_agent_config();
            break;
        case CONFIG_NETWORK:
            BK_LOGW(TAG, "Start to config network!");

            bk_genie_prepare_for_smart_config();
            break;
        // 其他事件处理...
        default:
            break;
    }
}

KeyConfig_t key_config[] =
{
    {
        .gpio_id = 13,
        .active_level = LOW_LEVEL_TRIGGER,
        .short_event = VOLUME_UP,
        .double_event = POWER_ON,   //TRICK: at shutdown mode, it can't recognize double press,short_event is really power on.(but short event is used by VOLUME UP when system is active).
        .long_event = SHUT_DOWN
    },
    {
        .gpio_id = 12,
        .active_level = LOW_LEVEL_TRIGGER,
        .short_event = VOLUME_DOWN,
        .double_event = VOLUME_DOWN,
        .long_event = CONFIG_NETWORK
    },
    {
        .gpio_id = 8,
        .active_level = LOW_LEVEL_TRIGGER,
        .short_event = AI_AGENT_CONFIG,
        .double_event = AI_AGENT_CONFIG,
        .long_event = AI_AGENT_CONFIG
    }
};

static void bk_key_register_wakeup_source()
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

#endif

void user_app_main(void)
{
#if (CONFIG_SYS_CPU0)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);

    agora_rtc_cli_init();
#endif

}

int main(void)
{
    if (bk_misc_get_reset_reason() != RESET_SOURCE_FORCE_DEEPSLEEP)
    {
#if (CONFIG_SYS_CPU0)
        rtos_set_user_app_entry((beken_thread_function_t)user_app_main);
#endif
        bk_init();

        media_service_init();

#if (CONFIG_SYS_CPU0)
        app_event_init();
#endif

#if (CONFIG_SYS_CPU1)
        agora_rtc_cli_init();
#endif

#if (CONFIG_SYS_CPU0)
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_AUDP_AUDIO, PM_POWER_MODULE_STATE_ON);

#ifdef CONFIG_LDO3V3_ENABLE
        BK_LOG_ON_ERR(gpio_dev_unmap(LDO3V3_CTRL_GPIO));
        bk_gpio_disable_pull(LDO3V3_CTRL_GPIO);
        bk_gpio_enable_output(LDO3V3_CTRL_GPIO);
        bk_gpio_set_output_high(LDO3V3_CTRL_GPIO);
#endif

        bk_genie_core_init();

#if CONFIG_NETWORK_AUTO_RECONNECT
        bk_genie_smart_config_init();
#endif

        register_event_handler(handle_system_event);

        bk_key_driver_init(key_config, sizeof(key_config) / sizeof(KeyConfig_t));
#endif

#if CONFIG_USBD_MSC
        extern void msc_storage_init(void);
        msc_storage_init();
#endif
    }
    else
    {

#if (CONFIG_SYS_CPU0)
        bk_init();
#if CONFIG_GSENSOR_ENABLE
        extern bk_err_t gsensor_demo_lowpower_wakeup();
        gsensor_demo_lowpower_wakeup();
        rtos_delay_milliseconds(50);
#endif
        bk_printf("RESET_SOURCE_FORCE_DEEPSLEEP\r\n");
        bk_key_register_wakeup_source();
        bk_pm_clear_deep_sleep_modules_config(PM_POWER_MODULE_NAME_AUDP);
        bk_pm_clear_deep_sleep_modules_config(PM_POWER_MODULE_NAME_VIDP);
        bk_pm_sleep_mode_set(PM_MODE_DEEP_SLEEP);
#endif
    }
    return 0;
}
