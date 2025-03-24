#include <os/os.h>
#include "modules/pm.h"
#include "media/media_main.h"

//#include "FreeRTOS.h"
//#include "task.h"

static char *TAG = "main";

void app_cpu0_main(void)
{
    os_printf("%s\n", __func__);

    rtos_delay_milliseconds(1000);

    bk_pm_cp1_auto_power_down_state_set(0x0);
    bk_pm_module_vote_power_ctrl(PM_POWER_MODULE_NAME_CPU1, PM_POWER_MODULE_STATE_ON);
    extern void start_cpu1_core(void);
    start_cpu1_core();

    media_main();
}
