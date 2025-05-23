#include <os/os.h>
#include "lcd_act.h"
#include "media_app.h"
#include "media_evt.h"


uint8_t lvgl_app_init_flag = 0;

const lcd_open_t lcd_open =
{
    .device_ppi = PPI_360X360,
    .device_name = "st77916",
};

void lvgl_app_init(void)
{
    bk_err_t ret;

    if (lvgl_app_init_flag == 1)
    {
        os_printf("lvgl_app_init has inited\r\n");
        return;
    }

    ret = media_app_lvgl_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK)
    {
        os_printf("media_app_lvgl_open failed\r\n");
        return;
    }

    lvgl_app_init_flag = 1;
}

void lvgl_app_deinit(void)
{
    bk_err_t ret;

    if (lvgl_app_init_flag == 0)
    {
        os_printf("lvgl_app_deinit has deinited or init failed\r\n");
        return;
    }

    ret = media_app_lvgl_close();
    if (ret != BK_OK)
    {
        os_printf("media_app_lvgl_close failed\r\n");
        return;
    }

    lvgl_app_init_flag = 0;
}

