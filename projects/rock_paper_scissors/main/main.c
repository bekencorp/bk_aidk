#include <os/os.h>
#include "bk_private/bk_init.h"
#include "media_service.h"
#include <driver/gpio.h>
#include "gpio_driver.h"

#ifdef CONFIG_LDO3V3_ENABLE
#ifndef LDO3V3_CTRL_GPIO
#ifdef CONFIG_LDO3V3_CTRL_GPIO
#define LDO3V3_CTRL_GPIO    CONFIG_LDO3V3_CTRL_GPIO
#else
#define LDO3V3_CTRL_GPIO    GPIO_52
#endif
#endif
#endif

int main(void)
{

    //bk_set_printf_sync(true);

#if (CONFIG_SYS_CPU0)
    // extern void app_main(void);
    // extern void rtos_set_user_app_entry(beken_thread_function_t entry);
    // rtos_set_user_app_entry((beken_thread_function_t)app_main);
#endif
    bk_init();
    media_service_init();

#if (CONFIG_SYS_CPU0)
#ifdef CONFIG_LDO3V3_ENABLE
    BK_LOG_ON_ERR(gpio_dev_unmap(LDO3V3_CTRL_GPIO));
    bk_gpio_disable_pull(LDO3V3_CTRL_GPIO);
    bk_gpio_enable_output(LDO3V3_CTRL_GPIO);
    bk_gpio_set_output_high(LDO3V3_CTRL_GPIO);
#endif

    extern void lvgl_app_init(void);
    lvgl_app_init();

    extern void app_cpu0_main(void);
    app_cpu0_main();

#elif (CONFIG_SYS_CPU1)
    extern void app_cpu1_main(void);
    app_cpu1_main();
    os_printf("\n cpu1 main\n");
#endif

    return 0;
}
