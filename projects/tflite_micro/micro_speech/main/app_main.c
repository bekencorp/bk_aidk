#include <string.h>

#include <os/os.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "modules/pm.h"

#include "aud_intf.h"

#include "media_app.h"
#include "media_evt.h"
#include "media_service.h"

#include "bk_gpio.h"
#include <driver/gpio.h>
#include <driver/hal/hal_gpio_types.h>
#include "gpio_driver.h"


static char *TAG = "main";

#define LOGD(TAG, format, ...) os_printf("\033[0;32m" format "\n\033[0m", ##__VA_ARGS__)
#define LOGI(TAG, format, ...) os_printf("\033[0;32m" format "\n\033[0m", ##__VA_ARGS__)
#define LOGE(TAG, format, ...) os_printf("\033[0;31m" format "\n\033[0m", ##__VA_ARGS__)

static uint16_t HIGH_POWER[] = {5, 6, 8};
static uint16_t LOW_POWER[] = {12, 13};
#define HIGH_POWER_NUM       (sizeof(HIGH_POWER)/sizeof(HIGH_POWER[0]))
#define LOW_POWER_NUM       (sizeof(LOW_POWER)/sizeof(LOW_POWER[0]))

void app_main(void *arg)
{
    LOGI(TAG, "app_main\n");

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (size_t i = 0; i < HIGH_POWER_NUM; i++)
    {
        gpio_dev_unmap(HIGH_POWER[i]);
        BK_LOG_ON_ERR(bk_gpio_disable_input(HIGH_POWER[i]));
        BK_LOG_ON_ERR(bk_gpio_enable_output(HIGH_POWER[i]));
        bk_gpio_set_output_high(HIGH_POWER[i]);
    }

    for (size_t i = 0; i < LOW_POWER_NUM; i++)
    {
        gpio_dev_unmap(LOW_POWER[i]);
        BK_LOG_ON_ERR(bk_gpio_disable_input(LOW_POWER[i]));
        BK_LOG_ON_ERR(bk_gpio_enable_output(LOW_POWER[i]));
        bk_gpio_set_output_low(LOW_POWER[i]);
    }

    //为了开启CPU1
#if 1
    //不确定用法，会崩溃
    bk_pm_cp1_auto_power_down_state_set(0x0);
    bk_pm_module_vote_power_ctrl(PM_POWER_MODULE_NAME_CPU1, PM_POWER_MODULE_STATE_ON);
    extern void start_cpu1_core(void);
    start_cpu1_core();
#else
    // 项目图像数据不是通过cam获取的, 在image_provider.c
    media_camera_device_t camera_device = {0};
    camera_device.type = DVP_CAMERA;
    camera_device.mode = JPEG_YUV_MODE;
    camera_device.fmt = PIXEL_FMT_JPEG;
    camera_device.info.resolution.width = 640;
    camera_device.info.resolution.height = 480;
    camera_device.info.fps = FPS20;
    media_app_camera_open(&camera_device);
#endif

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));


        //os_printf("freq:%d\r\n", bk_pm_module_current_cpu_freq_get(PM_DEV_ID_LIN));
    }
}


