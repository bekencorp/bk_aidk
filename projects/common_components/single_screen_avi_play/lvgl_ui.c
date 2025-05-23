#include <os/os.h>
#include "lcd_act.h"
#include "lv_vendor.h"
#include "lvgl.h"
#include "driver/drv_tp.h"
#include "yuv_encode.h"
#include "media_evt.h"
#include "bk_avi_play.h"

#define TAG "lvgl_ui"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static lv_vnd_config_t lv_vnd_config = {0};

bk_err_t lvgl_event_close_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s \r\n", __func__);

    lv_vendor_disp_lock();
    bk_avi_play_stop();

    lv_vendor_disp_unlock();
    lv_vendor_stop();
    lcd_display_close();

    bk_avi_play_close();

    return BK_OK;
}

bk_err_t lvgl_event_open_handle(media_mailbox_msg_t *msg)
{
    bk_err_t ret = BK_FAIL;

    LOGI("%s \n", __func__);

    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;

    if (lv_vnd_config.draw_pixel_size == 0) {
#ifdef CONFIG_LVGL_USE_PSRAM
#define PSRAM_DRAW_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
        lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi);
        lv_vnd_config.draw_buf_2_1 = (lv_color_t *)PSRAM_DRAW_BUFFER;
        lv_vnd_config.draw_buf_2_2 = (lv_color_t *)(PSRAM_DRAW_BUFFER + lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
#else
#define PSRAM_FRAME_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
        lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) / 10;
        lv_vnd_config.draw_buf_2_1 = LV_MEM_CUSTOM_ALLOC(lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
        lv_vnd_config.draw_buf_2_2 = NULL;
        lv_vnd_config.frame_buf_1 = (lv_color_t *)PSRAM_FRAME_BUFFER;
        lv_vnd_config.frame_buf_2 = NULL;//(lv_color_t *)(PSRAM_FRAME_BUFFER + ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * sizeof(lv_color_t));
#endif
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1 || CONFIG_LCD_QSPI_DEVICE_NUM > 1)
        lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
        lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi) * 2;
#else
        lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
        lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi);
#endif
        lv_vnd_config.rotation = ROTATE_NONE;

        lv_vendor_init(&lv_vnd_config);
    }

    lcd_display_open(lcd_open);

#if (CONFIG_TP)
    drv_tp_open(ppi_to_pixel_x(lcd_open->device_ppi), ppi_to_pixel_y(lcd_open->device_ppi), TP_MIRROR_NONE);
#endif

    ret = bk_avi_play_open("/resource.avi", false, true);
    if (ret != BK_OK)
    {
        LOGE("%s bk_avi_play_open failed\r\n", __func__);
        lcd_display_close();
        return ret;
    }

    lv_vendor_disp_lock();
    bk_avi_play_start();
    lv_vendor_disp_unlock();

    lv_vendor_start();

    return BK_OK;
}

void lvgl_event_handle(media_mailbox_msg_t *msg)
{
    bk_err_t ret = BK_FAIL;

    switch (msg->event)
    {
        case EVENT_LVGL_OPEN_IND:
            ret = lvgl_event_open_handle(msg);
            break;

        case EVENT_LVGL_CLOSE_IND:
            ret = lvgl_event_close_handle(msg);
            break;

        default:
            break;
    }

    msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
}

