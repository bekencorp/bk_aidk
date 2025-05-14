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
#include "audio_transfer.h"
extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);
extern int bk_cli_init(void);
#if CONFIG_BEKEN_GENIE_CORE
extern int bk_genie_main(void);
extern void bk_enter_deepsleep();
#endif
#define TAG "APP_MAIN"

void user_app_main(void)
{
#if (CONFIG_SYS_CPU0)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_240M);

    extern int byte_rtc_cli_init(void);
    byte_rtc_cli_init();
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

        bk_genie_main();
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
