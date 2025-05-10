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
#include "modules/avilib.h"
#include "modules/jpeg_decode_sw.h"
#include "media_evt.h"


#define TAG "AVI"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#if (CONFIG_SYS_CPU1)
#include "lv_jpeg_hw_decode.h"

#define AVI_VIDEO_USE_HW_DECODE    1
#define AVI_VIDEO_MAX_FRAME_LEN    (30 * 1024)


typedef struct
{
    avi_t *avi;
    uint32_t *video_frame;
    uint32_t video_len;
    uint32_t video_num;

    uint16_t *framebuffer;
    uint16_t *segmentbuffer;

    uint32_t frame_size;
    uint8_t video_segment_flag;
    uint32_t pos;
} bk_avi_play_t;

static bk_avi_play_t bk_avi_play = {0};
static lv_vnd_config_t lv_vnd_config = {0};

static lv_obj_t *img = NULL;
static lv_timer_t *timer = NULL;
static lv_img_dsc_t img_dsc =
{
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .data_size = 0,
    .data = NULL,
};

static lv_obj_t *label1 = NULL;
static lv_obj_t *label2 = NULL;
static uint8_t lvgl_ui_index = 0;

static void bk_avi_play_config_free(bk_avi_play_t *avi_play)
{
    if (avi_play->avi)
    {
        AVI_close(avi_play->avi);
        avi_play->avi = NULL;
    }

    if (avi_play->video_frame)
    {
        psram_free(avi_play->video_frame);
        avi_play->video_frame = NULL;
    }

    if (avi_play->framebuffer)
    {
        psram_free(avi_play->framebuffer);
        avi_play->framebuffer = NULL;
    }

    if (avi_play->segmentbuffer)
    {
        psram_free(avi_play->segmentbuffer);
        avi_play->segmentbuffer = NULL;
    }

    avi_play->pos = 0;
}

static bk_err_t bk_avi_play_open(bk_avi_play_t *avi_play, const char *filename, uint8_t segment_flag)
{
    LOGI("%s [%d]\r\n", __func__, __LINE__);

    os_memset(avi_play, 0x00, sizeof(bk_avi_play_t));

    lv_vendor_fs_init();

    avi_play->avi = AVI_open_input_file(filename, 1);
    if (avi_play->avi == NULL)
    {
        LOGE("%s open avi file failed\r\n", __func__);
        return BK_FAIL;
    }
    else
    {
        avi_play->video_num = AVI_video_frames(avi_play->avi);
        avi_play->frame_size = avi_play->avi->width * avi_play->avi->height * 2;
        LOGI("avi video_num: %d, width: %d, height: %d, frame_size: %d, fps: %d\r\n", avi_play->video_num, avi_play->avi->width, avi_play->avi->height, avi_play->frame_size, (uint32_t)avi_play->avi->fps);
    }

    avi_play->pos = 0;
    avi_play->video_segment_flag = segment_flag;

    avi_play->video_frame = psram_malloc(AVI_VIDEO_MAX_FRAME_LEN);
    if (avi_play->video_frame == NULL)
    {
        LOGE("%s video_frame malloc fail\r\n", __func__);
        AVI_close(avi_play->avi);
        return BK_FAIL;
    }

    avi_play->framebuffer = psram_malloc(avi_play->frame_size);
    if (avi_play->framebuffer == NULL)
    {
        LOGE("%s framebuffer malloc fail\r\n", __func__);
        goto out;
    }

    if (avi_play->video_segment_flag == 1)
    {
        avi_play->segmentbuffer = psram_malloc(avi_play->frame_size);
        if (avi_play->segmentbuffer == NULL)
        {
            LOGE("%s %d segmentbuffer malloc fail\r\n", __func__, __LINE__);
            goto out;
        }
    }

#if AVI_VIDEO_USE_HW_DECODE
    bk_jpeg_hw_decode_to_mem_init();
#else
    jd_output_format format = {0};

    bk_jpeg_dec_sw_init(NULL, 0);
    format.format = JD_FORMAT_RGB565;
    format.scale = 0;
    format.byte_order = JD_BIG_ENDIAN;
    jd_set_output_format(&format);
#endif
    LOGI("%s complete\r\n", __func__);

    return BK_OK;
out:
    bk_avi_play_config_free(avi_play);

    return BK_FAIL;
}

static void bk_avi_play_close(bk_avi_play_t *avi_play)
{
#if AVI_VIDEO_USE_HW_DECODE
    bk_jpeg_hw_decode_to_mem_deinit();
#else
    bk_jpeg_dec_sw_deinit();
#endif

    bk_avi_play_config_free(avi_play);

    lv_vendor_fs_deinit();

    LOGI("%s complete\r\n", __func__);
}

static bk_err_t bk_avi_video_prase_to_rgb565(bk_avi_play_t *avi_play)
{
    bk_err_t ret = BK_OK;

    ret = AVI_set_video_position(avi_play->avi, avi_play->pos, (long *)&avi_play->video_len);
    if (ret != BK_OK)
    {
        LOGE("%s %d AVI_set_video_position failed\r\n", __func__, __LINE__);
        return ret;
    }

    AVI_read_frame(avi_play->avi, (char *)avi_play->video_frame, avi_play->video_len);

    if (avi_play->video_len == 0)
    {
        avi_play->pos = avi_play->pos + 1;
        ret = AVI_set_video_position(avi_play->avi, avi_play->pos, (long *)&avi_play->video_len);
        if (ret != BK_OK)
        {
            LOGE("%s %d AVI_set_video_position failed\r\n", __func__, __LINE__);
            return ret;
        }

        AVI_read_frame(avi_play->avi, (char *)avi_play->video_frame, avi_play->video_len);
    }

#if AVI_VIDEO_USE_HW_DECODE
    ret = bk_jpeg_hw_decode_to_mem((uint8_t *)avi_play->video_frame, (uint8_t *)avi_play->framebuffer, avi_play->video_len, avi_play->avi->width, avi_play->avi->height);
    if (ret != BK_OK)
    {
        LOGE("%s %d bk_jpeg_hw_decode_to_mem failed\r\n", __func__, __LINE__);
        return ret;
    }

    uint16_t *buf16 = (uint16_t *)avi_play->framebuffer;
    for (int k = 0; k < avi_play->avi->width * avi_play->avi->height; k++)
    {
        buf16[k] = ((buf16[k] & 0xff00) >> 8) | ((buf16[k] & 0x00ff) << 8);
    }
#else
    sw_jpeg_dec_res_t result;
    bk_jpeg_dec_sw_start(JPEGDEC_BY_FRAME, (uint8_t *)avi_play->video_frame, (uint8_t *)avi_play->framebuffer, avi_play->video_len, avi_play->frame_size, (sw_jpeg_dec_res_t *)&result);
#endif

    if (avi_play->video_segment_flag == 1)
    {
        for (int i = 0; i < avi_play->avi->height; i++)
        {
            os_memcpy(avi_play->segmentbuffer + i * (avi_play->avi->width >> 1), avi_play->framebuffer + i * avi_play->avi->width, avi_play->avi->width);
            os_memcpy(avi_play->segmentbuffer + (avi_play->avi->width >> 1) * avi_play->avi->height + i * (avi_play->avi->width >> 1), avi_play->framebuffer + i * avi_play->avi->width + (avi_play->avi->width >> 1), avi_play->avi->width);
        }
    }

    return ret;
}

static void lv_timer_cb(lv_timer_t *timer)
{
    bk_err_t ret = BK_OK;

    bk_avi_play.pos++;
    if (bk_avi_play.pos >= bk_avi_play.video_num)
    {
        bk_avi_play.pos = 0;
    }

    ret = bk_avi_video_prase_to_rgb565(&bk_avi_play);
    if (ret != BK_OK)
    {
        LOGE("%s %d bk_avi_video_prase_to_rgb565 failed\r\n", __func__, __LINE__);
        return;
    }

    lv_img_set_src(img, &img_dsc);
}

static void bk_avi_play_start(void)
{
    img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &img_dsc);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    timer = lv_timer_create(lv_timer_cb, 1000 / bk_avi_play.avi->fps, NULL);

    LOGI("%s complete\r\n", __func__);
}

static void bk_avi_play_stop(void)
{
    lv_timer_del(timer);
    timer = NULL;
    lv_obj_del(img);
    img = NULL;

    LOGI("%s complete\r\n", __func__);
}

bk_err_t lvgl_event_close_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s \r\n", __func__);

    lv_vendor_disp_lock();

    if (lvgl_ui_index == 1) {
        bk_avi_play_stop();
    }

    if (label1) {
        lv_obj_del(label1);
        label1 = NULL;
    }

    if (label2) {
        lv_obj_del(label2);
        label2 = NULL;
    }
    lv_vendor_disp_unlock();

    lvgl_ui_index = 0;

    lv_vendor_stop();
    lcd_display_close();

    bk_avi_play_close(&bk_avi_play);

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
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
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

    ret = bk_avi_play_open(&bk_avi_play, "/genie_eye.avi", 1);
    if (ret != BK_OK)
    {
        LOGE("%s bk_avi_play_open failed\r\n", __func__);
        lcd_display_close();
        return ret;
    }

#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
    img_dsc.header.w = lv_vnd_config.lcd_hor_res;
    img_dsc.header.h = lv_vnd_config.lcd_ver_res;
    img_dsc.data_size = img_dsc.header.w * img_dsc.header.h * 2;
#else
    img_dsc.header.w = bk_avi_play.avi->width;
    img_dsc.header.h = bk_avi_play.avi->height;
    img_dsc.data_size = img_dsc.header.w * img_dsc.header.h * 2;
#endif

    if (bk_avi_play.video_segment_flag == 1)
    {
        img_dsc.data = (const uint8_t *)bk_avi_play.segmentbuffer;
    }
    else
    {
        img_dsc.data = (const uint8_t *)bk_avi_play.framebuffer;
    }

    ret = bk_avi_video_prase_to_rgb565(&bk_avi_play);
    if (ret != BK_OK)
    {
        LOGE("%s %d bk_avi_video_prase_to_rgb565 failed\r\n", __func__, __LINE__);
        return ret;
    }

    LV_FONT_DECLARE(lv_font);

    lv_vendor_disp_lock();

    bk_avi_play_start();

    label1 = lv_label_create(lv_scr_act());
    lv_label_set_text(label1, "图 像");
    lv_obj_set_style_text_font(label1, &lv_font, 0);
    lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_add_flag(label1, LV_OBJ_FLAG_HIDDEN);

    label2 = lv_label_create(lv_scr_act());
    lv_label_set_text(label2, "识 别");
    lv_obj_set_style_text_font(label2, &lv_font, 0);
    lv_obj_align(label2, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_add_flag(label2, LV_OBJ_FLAG_HIDDEN);

    lvgl_ui_index = 1;

    lv_vendor_disp_unlock();

    lv_vendor_start();

    return BK_OK;
}

bk_err_t lvgl_event_switch_ui_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s \r\n", __func__);

    if (lvgl_ui_index == 1) {
        lv_vendor_disp_lock();
        bk_avi_play_stop();

        lv_obj_clear_flag(label1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(label2, LV_OBJ_FLAG_HIDDEN);
        lvgl_ui_index = 2;
        lv_vendor_disp_unlock();
    } else if (lvgl_ui_index == 2) {
        lv_vendor_disp_lock();
        lv_obj_add_flag(label1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label2, LV_OBJ_FLAG_HIDDEN);

        bk_avi_play_start();
        lvgl_ui_index = 1;
        lv_vendor_disp_unlock();
    } else {
        LOGE("%s index is invalid\r\n", __func__);
    }

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

        case EVENT_LVGL_SWITCH_UI_IND:
            ret = lvgl_event_switch_ui_handle(msg);
            break;

        default:
            break;
    }

    msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
}
#endif

#if (CONFIG_SYS_CPU0)
uint8_t lvgl_app_init_flag = 0;

const lcd_open_t lcd_open =
{
    .device_ppi = PPI_160X160,
    .device_name = "gc9d01",
};

void lvgl_app_init(void)
{
    bk_err_t ret;

    if (lvgl_app_init_flag == 1)
    {
        LOGW("lvgl_app_init has inited\r\n");
        return;
    }

    ret = media_app_lvgl_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK)
    {
        LOGE("media_app_lvgl_open failed\r\n");
        return;
    }

    lvgl_app_init_flag = 1;
}

void lvgl_app_deinit(void)
{
    bk_err_t ret;

    if (lvgl_app_init_flag == 0)
    {
        LOGW("lvgl_app_deinit has deinited or init failed\r\n");
        return;
    }

    ret = media_app_lvgl_close();
    if (ret != BK_OK)
    {
        LOGE("media_app_lvgl_close failed\r\n");
        return;
    }

    lvgl_app_init_flag = 0;
}

#endif

