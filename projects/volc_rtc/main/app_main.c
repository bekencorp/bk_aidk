#include <common/bk_include.h>
#include <components/log.h>
#include <components/event.h>
#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>
#include "cli.h"
#include "sys_driver.h"
#include "sys_hal.h"
#include <string.h>
#if (CONFIG_SYS_CPU0)
#include "VolcEngineRTCLite.h"
#endif

extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);
extern int bk_cli_init(void);
#if CONFIG_BEKEN_GENIE_CORE
extern int bk_genie_main(void);
extern void bk_enter_deepsleep();
#endif

#define TAG "APP_MAIN"



// #if (CONFIG_SYS_CPU0)
// #define BEKEN_RTC_CMD_CNT   (sizeof(s_beken_rtc_commands) / sizeof(struct cli_command))

// extern void cli_beken_rtc_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
// extern void cli_beken_rtc_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
// extern void lvgl_app_init(void);

// static const struct cli_command s_beken_rtc_commands[] =
// {
//     {"rtc_test", "rtc_test ...", cli_beken_rtc_test_cmd},
//     {"rtc_debug", "rtc_debug ...", cli_beken_rtc_debug_cmd},
// #if CONFIG_NETWORK_AUTO_RECONNECT
//     {"bk_smart_config_erase", "bk_smart_config_erase", bk_genie_smart_config_cli}
// #endif
// };

// #if (CONFIG_SYS_CPU0)
// static const uint32_t s_user_value2 = 10;
// const struct factory_config_t s_user_config[] = {
//     {"user_key1", (void *)"user_value1", 11, BK_FALSE, 0},
//     {"user_key2", (void *)&s_user_value2, 4, BK_TRUE, 4},
// };
// #endif

// static int beken_rtc_cli_init(void)
// {
//     return cli_register_commands(s_beken_rtc_commands, BEKEN_RTC_CMD_CNT);
// }

// #elif CONFIG_SYS_CPU1
// #define BEKEN_RTC_CMD_CNT   (sizeof(s_beken_rtc_commands) / sizeof(struct cli_command))
// extern void cli_beken_rtc_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
// static const struct cli_command s_beken_rtc_commands[] =
// {
//     {"rtc_debug", "rtc_debug ...", cli_beken_rtc_debug_cmd},
// };

// static int beken_rtc_cli_init(void)
// {
//     return cli_register_commands(s_beken_rtc_commands, BEKEN_RTC_CMD_CNT);
// }

// #endif

void beken_auto_run(void) {

}

bk_err_t beken_rtc_stop(void) {
    return BK_OK;
}

bk_err_t audio_turn_on(void) {
    return BK_OK;
}

void user_app_main(void)
{
#if (CONFIG_SYS_CPU0)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_240M);

    // beken_rtc_cli_init();
    os_printf("VolcEngineRTCLite lib version:%s\r\n", byte_rtc_get_version());
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

// #if (CONFIG_SYS_CPU1)
//         beken_rtc_cli_init();
// #endif

#if CONFIG_BEKEN_GENIE_CORE
        bk_genie_main();
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
