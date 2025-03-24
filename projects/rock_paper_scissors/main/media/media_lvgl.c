#if 1 //CONFIG_SYS_CPU1
#include "os/os.h"
#include "os/str.h"
#include "os/mem.h"
#include "media_main.h"

#include "lcd_act.h"
#include "media_app.h"
#include "media_evt.h"

#include "frame_buffer.h"
//#include "lcd_display_service.h"
#include "yuv_encode.h"
#include "lv_vendor.h"
#include "driver/media_types.h"
#include "lvgl.h"

#if CONFIG_MEDIA_PIPELINE
extern uint8_t lvgl_disp_enable;
#else
static uint8_t lvgl_disp_enable;
#endif

extern lv_vnd_config_t vendor_config;
extern frame_buffer_t *lvgl_frame_buffer;
static lv_obj_t *qr;

static void lv_example_qrcode(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lv_scr_act(), 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_obj_set_x(label, 0);
    lv_obj_set_y(label, 200);
    lv_obj_set_align(label, LV_ALIGN_TOP_MID);
    lv_label_set_text(label, "Welcome to BEKEN");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
    lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);
    qr = lv_qrcode_create(lv_scr_act(), 260, fg_color, bg_color);

    /*Set data*/
    const char *data = "http://www.bekencorp.com";
    lv_qrcode_update(qr, data, os_strlen(data));
    lv_obj_align(qr, LV_ALIGN_CENTER, 0, 40);

    /*Add a border with bg_color*/
    lv_obj_set_style_border_color(qr, bg_color, 0);
    lv_obj_set_style_border_width(qr, 5, 0);
}

static void lvgl_event_open_handle(media_mailbox_msg_t *msg)
{
    os_printf("%s EVENT_LVGL_OPEN_IND \n", __func__);

    lvgl_disp_enable = 1;

    lv_vnd_config_t lv_vnd_config = {0};
    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;

    frame_buffer_t *lv_frame_buffer = frame_buffer_display_malloc(ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * sizeof(lv_color_t));

    if (lv_frame_buffer == NULL)
    {
        os_printf("[%s] lv_frame_buffer malloc fail\r\n", __func__);
        msg_send_rsp_to_media_major_mailbox(msg, BK_FAIL, APP_MODULE);
        return;
    }

    lv_vnd_config.draw_pixel_size = (60 * 1024) / sizeof(lv_color_t);
    lv_vnd_config.draw_buf_2_1 = LV_MEM_CUSTOM_ALLOC(lv_vnd_config.draw_pixel_size *sizeof(lv_color_t));
    lv_vnd_config.draw_buf_2_2 = NULL;
    lv_vnd_config.frame_buf_1 = (lv_color_t *)lv_frame_buffer->frame;
    lv_vnd_config.frame_buf_2 = NULL;

    lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
    lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi);
    lv_vnd_config.rotation = ROTATE_NONE;

#if (CONFIG_TP)
    drv_tp_open(ppi_to_pixel_x(lcd_open->device_ppi), ppi_to_pixel_y(lcd_open->device_ppi), TP_MIRROR_NONE);
#endif

    lv_vendor_init(&lv_vnd_config);

    lv_vendor_disp_lock();
    lv_example_qrcode();
    lv_vendor_disp_unlock();

    lv_vendor_start();

    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}

static void lvgl_event_close_handle(media_mailbox_msg_t *msg)
{
    lv_vendor_stop();

    lvgl_disp_enable = 0;

    lv_vendor_deinit();

#if (CONFIG_TP)
    drv_tp_close();
#endif

    os_printf("%s\r\n", __func__);

    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}

static void lvgl_event_lvcam_lvgl_open_handle(media_mailbox_msg_t *msg)
{
    lvgl_disp_enable = 1;

    lv_vendor_start();

    //    lv_vendor_disp_lock();
    //    lv_example_qrcode();
    //    lv_vendor_disp_unlock();

    //if you return to displaying a static image, no need to redraw, otherwise you need to redraw ui.
    lcd_display_frame_request(lvgl_frame_buffer);

    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}

static void lvgl_event_lvcam_lvgl_close_handle(media_mailbox_msg_t *msg)
{
    lv_vendor_stop();
    //    lv_qrcode_delete(qr);
    lvgl_disp_enable = 0;
    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}


void lvgl_event_handle(media_mailbox_msg_t *msg)
{
    appm_logi("evt 0x%x", msg->event);

    switch (msg->event)
    {
    case EVENT_LVGL_OPEN_IND:
        lvgl_event_open_handle(msg);
        break;

    case EVENT_LVGL_CLOSE_IND:
        lvgl_event_close_handle(msg);
        break;

    case EVENT_LVCAM_LVGL_OPEN_IND:
        lvgl_event_lvcam_lvgl_open_handle(msg);
        break;

    case EVENT_LVCAM_LVGL_CLOSE_IND:
        lvgl_event_lvcam_lvgl_close_handle(msg);
        break;

    default:
        break;
    }
}

#endif
