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
#define AGORA_RTC_CMD_CNT   (sizeof(s_agora_rtc_commands) / sizeof(struct cli_command))

extern void cli_agora_rtc_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
extern void cli_agora_rtc_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
extern void lvgl_app_init(void);

static const struct cli_command s_agora_rtc_commands[] =
{
    {"agora_test", "agora_test ...", cli_agora_rtc_test_cmd},
    {"agora_debug", "agora_debug ...", cli_agora_rtc_debug_cmd},
};

static int agora_rtc_cli_init(void)
{
    return cli_register_commands(s_agora_rtc_commands, AGORA_RTC_CMD_CNT);
}

/* connect wifi by cli */
#if CONFIG_WIFI_AUTO_RECONNECT
int demo_save_wifi_reconnect_info(netif_if_t type, void *val);
static int netif_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    netif_event_got_ip4_t *got_ip;
    __maybe_unused wifi_sta_config_t sta_config = {0};

    switch (event_id)
    {
        case EVENT_NETIF_GOT_IP4:
            got_ip = (netif_event_got_ip4_t *)event_data;
            BK_LOGI(TAG, "%s got ip %s.\n", got_ip->netif_if == NETIF_IF_STA ? "STA" : "unknown netif", got_ip->ip);
            bk_wifi_sta_get_config(&sta_config);
            demo_save_wifi_reconnect_info(NETIF_IF_STA, &sta_config);
            //extern void agora_auto_run(void);
            //agora_auto_run();
            break;
        default:
            BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}

int wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    wifi_event_sta_disconnected_t *sta_disconnected;
    wifi_event_sta_connected_t *sta_connected;

    switch (event_id)
    {
        case EVENT_WIFI_STA_CONNECTED:
            sta_connected = (wifi_event_sta_connected_t *)event_data;
            BK_LOGI(TAG, "STA connected to %s\n", sta_connected->ssid);
            break;

        case EVENT_WIFI_STA_DISCONNECTED:
            sta_disconnected = (wifi_event_sta_disconnected_t *)event_data;
            BK_LOGI(TAG, "STA disconnected, reason(%d)\n", sta_disconnected->disconnect_reason);
            break;

        default:
            BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}


static void event_handler_init(void)
{
    BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, wifi_event_cb, NULL));
    BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_NETIF, EVENT_ID_ALL, netif_event_cb, NULL));
}

extern void demo_wifi_fast_connect(void);
#endif
#endif

#if (CONFIG_SYS_CPU0)
// 按键 1 的回调函数
void volume_increase()
{
    BK_LOGI(TAG, " volume up\r\n");
}

void volume_decrease()
{
    BK_LOGI(TAG, " volume down\r\n");
}

void power_off()
{
    BK_LOGI(TAG, " power_off\r\n");

    BK_LOGW(TAG, " ************TODO:Just force deep sleep for Demo!\r\n");
    bk_pm_clear_deep_sleep_modules_config(PM_POWER_MODULE_NAME_AUDP);
    bk_pm_clear_deep_sleep_modules_config(PM_POWER_MODULE_NAME_VIDP);
    bk_pm_sleep_mode_set(PM_MODE_DEEP_SLEEP);
}

void power_on()
{
    BK_LOGI(TAG, "power_on\r\n");
}

void ai_agent_config()
{
    BK_LOGW(TAG, " ************TODO:AI Agent doesn't complete!\r\n");
}

// 业务的实现增加在里面，在key_config结构体里面填写对应的业务
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
#endif

void user_app_main(void)
{
#if (CONFIG_SYS_CPU0)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);

#if CONFIG_WIFI_AUTO_RECONNECT
    event_handler_init();
    demo_wifi_fast_connect();
#endif

    bk_genie_core_init();
    agora_rtc_cli_init();
#endif
}

int main(void)
{
#if (CONFIG_SYS_CPU0)
    rtos_set_user_app_entry((beken_thread_function_t)user_app_main);
#endif
    bk_init();

    media_service_init();

#if (CONFIG_SYS_CPU0)
    bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_AUDP_AUDIO, PM_POWER_MODULE_STATE_ON);

#ifdef CONFIG_LDO3V3_ENABLE
    BK_LOG_ON_ERR(gpio_dev_unmap(LDO3V3_CTRL_GPIO));
    bk_gpio_disable_pull(LDO3V3_CTRL_GPIO);
    bk_gpio_enable_output(LDO3V3_CTRL_GPIO);
    bk_gpio_set_output_high(LDO3V3_CTRL_GPIO);
#endif

    lvgl_app_init();


    register_event_handler(handle_system_event);

    bk_key_driver_init(key_config, sizeof(key_config) / sizeof(KeyConfig_t));
#endif

#if CONFIG_USBD_MSC
    extern void msc_storage_init(void);
    msc_storage_init();
#endif
    return 0;
}
