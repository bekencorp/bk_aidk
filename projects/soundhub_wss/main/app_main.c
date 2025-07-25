#include <common/sys_config.h>
#include <components/log.h>
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
#include <key_app_service.h>
#include "sys_driver.h"
#include "sys_hal.h"
#include <driver/gpio.h>
#include "gpio_driver.h"

#if (CONFIG_SYS_CPU0)
#include "aud_intf.h"
#include "bk_factory_config.h"
#if CONFIG_BK_SMART_CONFIG
#include "bk_genie_comm.h"
#include "bk_smart_config.h"
#endif
#include "motor.h"
#include "audio_engine.h"
#include "video_engine.h"
#include "network_transfer.h"
#elif (CONFIG_SYS_CPU1)
#include "audio_engine.h"
#include "video_engine.h"
#else
#endif

#include "media_app.h"
#include "app_event.h"
#include "countdown.h"
#include <led_blink.h>
#include <common/bk_include.h>
#include "components/bluetooth/bk_dm_bluetooth.h"
#if CONFIG_NET_PAN
#include "bluetooth_storage.h"
#endif
#include "app_main.h"
#include "app_evt_process.h"

extern void rtos_set_user_app_entry(beken_thread_function_t entry);
extern int bk_cli_init(void);
extern void bk_set_jtag_mode(uint32_t cpu_id, uint32_t group_id);

#define TAG "WSS"

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
static const uint32_t s_user_value2 = 10;
#if CONFIG_NET_PAN
static const bt_user_storage_t s_bt_factory_storage ={0};
#endif
const struct factory_config_t s_user_config[] = {
    {"user_key1", (void *)"user_value1", 11, BK_FALSE, 0},
    {"user_key2", (void *)&s_user_value2, 4, BK_TRUE, 4},
#if CONFIG_NET_PAN
    {BT_STORAGE_KEY, (void *)&s_bt_factory_storage, sizeof(s_bt_factory_storage), BK_TRUE, sizeof(s_bt_factory_storage)},
#endif
};
#endif


#if (CONFIG_SYS_CPU0)

static bk_err_t app_force_analog_ldo_gpio_close(void)
{
    /*audio*/
    sys_hal_set_ana_reg18_value(0);
    sys_hal_set_ana_reg19_value(0);
    sys_hal_set_ana_reg20_value(0);
    sys_hal_set_ana_reg21_value(0);
    sys_hal_set_ana_reg27_value(0);
    sys_drv_aud_aud_en(0);
    sys_drv_aud_audbias_en(0);
    sys_drv_apll_en(0);

   	/*usb/psram ldo ctrl at deepsleep last location*/
    /*ldo*/
    gpio_dev_unmap(GPIO_50);
    gpio_dev_unmap(GPIO_52);

    /*UART*/
    gpio_dev_unmap(GPIO_10);
    gpio_dev_unmap(GPIO_11);

    /*I2C*/
    gpio_dev_unmap(GPIO_0);
    gpio_dev_unmap(GPIO_1);

    /*MOTO*/
    gpio_dev_unmap(GPIO_9);

    return 0;
}

static void bk_enter_deepsleep(void)
{
#if CONFIG_GSENSOR_ENABLE
    extern int gsensor_enter_sleep_config();
    gsensor_enter_sleep_config();
    rtos_delay_milliseconds(10);
#endif

    BK_LOGI(TAG,"RESET_SOURCE_FORCE_DEEPSLEEP\r\n");
    bk_key_register_wakeup_source();
    bk_pm_clear_deep_sleep_modules_config(PM_POWER_MODULE_NAME_AUDP);
    bk_pm_clear_deep_sleep_modules_config(PM_POWER_MODULE_NAME_VIDP);
    app_force_analog_ldo_gpio_close();
    bk_pm_sleep_mode_set(PM_MODE_DEEP_SLEEP);
    rtos_delay_milliseconds(10);
}

static void bk_wait_power_on(void)
{
    uint32_t press_time = 0;

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    do {
        if (bk_gpio_get_input(KEY_GPIO_12) == 0) {
            extern void delay_ms(uint32 num);
            delay_ms(500);
            press_time += 500;

            if (bk_gpio_get_input(KEY_GPIO_12) != 0) {
                break;
            }
        } else {

            break;
        }
    } while (press_time < LONG_RRESS_TIMR);
    GLOBAL_INT_RESTORE();

    if (press_time < LONG_RRESS_TIMR)
    {
        // bk_key_register_wakeup_source();
        bk_enter_deepsleep();
    }
}
#endif

static void user_app_main(void)
{
#if (CONFIG_SYS_CPU0)

    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_240M);

    audio_engine_init();
    voide_engine_init();
    network_transfer_init();
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

#if (CONFIG_SYS_CPU0)
#ifdef CONFIG_LDO3V3_ENABLE
        BK_LOG_ON_ERR(gpio_dev_unmap(LDO3V3_CTRL_GPIO));
        bk_gpio_disable_pull(LDO3V3_CTRL_GPIO);
        bk_gpio_enable_output(LDO3V3_CTRL_GPIO);
        bk_gpio_set_output_high(LDO3V3_CTRL_GPIO);
#endif
#endif

#if (CONFIG_SYS_CPU0)
        /*to judgement key is long press or short press; long press exit deepsleep*/
        if(bk_misc_get_reset_reason() == RESET_SOURCE_DEEPPS_GPIO && (bk_gpio_get_wakeup_gpio_id() == KEY_GPIO_12))
        {
            //motor vibration
        #if CONFIG_MOTOR
            motor_open(PWM_MOTOR_CH_3);
        #endif

            bk_wait_power_on();

        #if CONFIG_MOTOR
            motor_close(PWM_MOTOR_CH_3);
        #endif
        }

        bk_regist_factory_user_config((const struct factory_config_t *)&s_user_config,
                                       sizeof(s_user_config)/sizeof(s_user_config[0]));
        bk_factory_init();
#endif

    //led init move before
#if (CONFIG_SYS_CPU0)

        //No operation countdown 3 minutes to shut down
        // start_countdown(countdown_ms);
        //led init move before
      #if CONFIG_LED_BLINK
          led_driver_init();
          led_app_set(LED_ON_GREEN,LED_LAST_FOREVER);
      #endif

#endif

        media_service_init();

#if (CONFIG_SYS_CPU0)
        extern void app_audio_arbiter_init(void);
        app_audio_arbiter_init();
#if (CONFIG_APP_EVT)
        app_event_init();
        app_evt_process_init();
#endif
#if (CONFIG_NFC_ENABLE)
        void nfc_get_id_task(void);
        nfc_get_id_task();
#endif
        volume_init();

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
        extern bk_err_t audio_turn_on(void);
        int ret = audio_turn_on();
        if (ret != BK_OK)
        {
            BK_LOGE(TAG, "%s, %d, audio turn on fail, ret:%d\n", __func__, __LINE__, ret);
        }
#endif
#endif

#if (CONFIG_SYS_CPU1)
        audio_engine_init();
#endif

#if (CONFIG_SYS_CPU0)
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_AUDP_AUDIO, PM_POWER_MODULE_STATE_ON);

#if CONFIG_BT
        extern int bt_manager_init(uint8_t is_visible);
        bt_manager_init(1);
#if CONFIG_A2DP_SINK_DEMO
        extern int a2dp_sink_demo_init(uint8_t aac_supported);
        a2dp_sink_demo_init(0);
#endif

#if CONFIG_HFP_HF_DEMO
        extern int hfp_hf_demo_init(uint8_t msbc_supported);
        hfp_hf_demo_init(0);
#endif
        extern int cli_headset_demo_init(void);
        cli_headset_demo_init();
#endif

#if CONFIG_BK_BOARDING_SERVICE
        bk_genie_core_init();
#endif
#if CONFIG_BK_SMART_CONFIG
        bk_smart_config_init();
#endif

#if CONFIG_BUTTON
        bk_key_service_init();
#endif

#if CONFIG_BAT_MONITOR
        extern void battery_monitor_init(void);
        battery_monitor_init();
#endif

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
        bk_enter_deepsleep();
#endif
    }

    return 0;
}
