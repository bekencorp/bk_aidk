#include <os/os.h>
#include "bk_private/bk_init.h"
#include "media_service.h"

int main(void)
{

    bk_set_printf_sync(true);

#if (CONFIG_SYS_CPU0)
    // extern void app_main(void);
    // extern void rtos_set_user_app_entry(beken_thread_function_t entry);
    // rtos_set_user_app_entry((beken_thread_function_t)app_main);
#endif
    bk_init();
    media_service_init();

#if (CONFIG_SYS_CPU0)
    os_printf("\n cpu0 main\n");
    extern void app_cpu0_main(void);
    app_cpu0_main();
#elif (CONFIG_SYS_CPU1)
    extern void app_cpu1_main(void);
    app_cpu1_main();
    os_printf("\n cpu1 main\n");
#endif

    return 0;
}