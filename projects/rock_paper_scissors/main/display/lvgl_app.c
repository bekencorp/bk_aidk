#include <os/os.h>
#include "lcd_act.h"
#include "media_app.h"
#if CONFIG_LVGL
#include "lv_vendor.h"
#include "lvgl.h"
#endif
#include "driver/drv_tp.h"
#include <driver/lcd.h>
#include "yuv_encode.h"


#define TAG "lvgl_app"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if (CONFIG_SYS_CPU1)
lv_obj_t *camera_img = NULL;
lv_obj_t *my_img = NULL;
lv_obj_t *label1 = NULL;
lv_obj_t *label2 = NULL;

void lvgl_event_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s EVENT_LVGL_OPEN_IND \n", __func__);

    lv_vnd_config_t lv_vnd_config = {0};
    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;

#ifdef CONFIG_LVGL_USE_PSRAM
#define PSRAM_DRAW_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
    lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi);
    lv_vnd_config.draw_buf_2_1 = (lv_color_t *)PSRAM_DRAW_BUFFER;
    lv_vnd_config.draw_buf_2_2 = (lv_color_t *)(PSRAM_DRAW_BUFFER + lv_vnd_config->draw_pixel_size * sizeof(lv_color_t));
#else
#define PSRAM_FRAME_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
    lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * 2 / 10;
    lv_vnd_config.draw_buf_2_1 = LV_MEM_CUSTOM_ALLOC(lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
    lv_vnd_config.draw_buf_2_2 = NULL;
    lv_vnd_config.frame_buf_1 = (lv_color_t *)PSRAM_FRAME_BUFFER;
    lv_vnd_config.frame_buf_2 = NULL;//(lv_color_t *)(PSRAM_FRAME_BUFFER + ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * sizeof(lv_color_t));
#endif
    lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
    lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi) * 2;
    lv_vnd_config.rotation = ROTATE_NONE;

    lv_vendor_init(&lv_vnd_config);

    lcd_display_open(lcd_open);

#if (CONFIG_TP)
    drv_tp_open(ppi_to_pixel_x(lcd_open->device_ppi), ppi_to_pixel_y(lcd_open->device_ppi), TP_MIRROR_NONE);
#endif

    LV_FONT_DECLARE(lv_custom_font);

    lv_vendor_disp_lock();
    camera_img = lv_img_create(lv_scr_act());
    lv_obj_set_pos(camera_img, 0, 0);
    lv_obj_add_flag(camera_img, LV_OBJ_FLAG_HIDDEN);
    label1 = lv_label_create(lv_scr_act());
    lv_label_set_text(label1, "猜 拳");
    lv_obj_set_style_text_font(label1, &lv_custom_font, 0);
    lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, 60);

    my_img = lv_img_create(lv_scr_act());
    lv_obj_set_pos(my_img, 0, 160);
    lv_obj_add_flag(my_img, LV_OBJ_FLAG_HIDDEN);
    label2 = lv_label_create(lv_scr_act());
    lv_label_set_text(label2, "游 戏");
    lv_obj_set_style_text_font(label2, &lv_custom_font, 0);
    lv_obj_align(label2, LV_ALIGN_BOTTOM_MID, 0, -60);

    lv_vendor_disp_unlock();

    lv_vendor_start();

    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}
#endif


#if (CONFIG_SYS_CPU0)
const lcd_open_t lcd_open =
{
    .device_ppi = PPI_160X160,
    .device_name = "gc9d01",
};

void lvgl_app_init(void)
{
    bk_err_t ret;

    ret = media_app_lvgl_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK)
    {
        LOGE("media_app_lvgl_open failed\r\n");
        return;
    }
}
#endif

