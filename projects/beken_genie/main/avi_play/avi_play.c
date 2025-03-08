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


#define TAG "AVI"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

const lcd_open_t lcd_open =
{
    .device_ppi = PPI_160X160,
    .device_name = "gc9d01",
};


#if (CONFIG_SYS_CPU1)
#include "lv_jpeg_hw_decode.h"

static avi_t *avi = NULL;
static uint32_t *video_frame = NULL;
static uint32_t video_num = 0;
static uint32_t video_len = 0;
static uint32_t pos = 0;
static uint16_t *framebuffer = NULL;
static uint16_t *segmentbuffer = NULL;
static uint32_t frame_size = 0;

static lv_obj_t *img = NULL;
static lv_img_dsc_t img_dsc =
{
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .data_size = 0,
    .data = NULL,
};

#define AVI_VIDEO_USE_HW_DECODE    1


static void avi_video_frame_parse_to_rgb565(avi_t *AVI, uint32_t frame, uint8_t *src_buf, uint8_t *dst_buf, uint32_t src_size, uint32_t outbuf_size)
{
    AVI_set_video_position(AVI, frame, (long *)&src_size);
    AVI_read_frame(AVI, (char *)src_buf, src_size);

    if (src_size == 0)
    {
        frame = frame + 1;
        AVI_set_video_position(AVI, frame, (long *)&src_size);
        AVI_read_frame(AVI, (char *)src_buf, src_size);
    }

#if AVI_VIDEO_USE_HW_DECODE
    bk_jpeg_hw_decode_to_mem(src_buf, dst_buf, src_size, avi->width, avi->height);

    uint16_t *buf16 = (uint16_t *)dst_buf;
    for (int k = 0; k < 320 * 160; k++)
    {
        buf16[k] = ((buf16[k] & 0xff00) >> 8) | ((buf16[k] & 0x00ff) << 8);
    }
#else
    sw_jpeg_dec_res_t result;
    bk_jpeg_dec_sw_start(JPEGDEC_BY_FRAME, src_buf, dst_buf, src_size, outbuf_size, (sw_jpeg_dec_res_t *)&result);
#endif
}

static void lv_timer_cb(lv_timer_t *timer)
{
    pos++;
    if (pos >= video_num)
    {
        pos = 0;
    }

    avi_video_frame_parse_to_rgb565(avi, pos, (uint8_t *)video_frame, (uint8_t *)framebuffer, video_len, frame_size);

    for (int i = 0; i < avi->height; i++)
    {
        os_memcpy(segmentbuffer + i * (avi->width >> 1), framebuffer + i * avi->width, avi->width);
        os_memcpy(segmentbuffer + (avi->width >> 1) * avi->height + i * (avi->width >> 1), framebuffer + i * avi->width + (avi->width >> 1), avi->width);
    }

    lv_img_set_src(img, &img_dsc);
}

void lvgl_event_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s EVENT_LVGL_OPEN_IND \n", __func__);

    lv_vnd_config_t lv_vnd_config = {0};
    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;
    jd_output_format *format = NULL;

    format = os_malloc(sizeof(jd_output_format));
    if (format == NULL)
    {
        LOGE("%s %d format malloc fail\r\n", __func__, __LINE__);
        msg_send_rsp_to_media_major_mailbox(msg, BK_FAIL, APP_MODULE);
        return;
    }

#if AVI_VIDEO_USE_HW_DECODE
    bk_jpeg_hw_decode_to_mem_init();
#else
    bk_jpeg_dec_sw_init(NULL, 0);
    format->format = JD_FORMAT_RGB565;
    format->scale = 0;
    format->byte_order = JD_BIG_ENDIAN;
    jd_set_output_format(format);
#endif

#ifdef CONFIG_LVGL_USE_PSRAM
#define PSRAM_DRAW_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
    lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi);
    lv_vnd_config.draw_buf_2_1 = (lv_color_t *)PSRAM_DRAW_BUFFER;
    lv_vnd_config.draw_buf_2_2 = (lv_color_t *)(PSRAM_DRAW_BUFFER + lv_vnd_config->draw_pixel_size * sizeof(lv_color_t));
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

    lcd_display_open(lcd_open);

#if (CONFIG_TP)
    drv_tp_open(ppi_to_pixel_x(lcd_open->device_ppi), ppi_to_pixel_y(lcd_open->device_ppi), TP_MIRROR_NONE);
#endif

    img_dsc.header.w = lv_vnd_config.lcd_hor_res;
    img_dsc.header.h = lv_vnd_config.lcd_ver_res;
    img_dsc.data_size = img_dsc.header.w * img_dsc.header.h * 2;

    video_frame = os_malloc(30 * 1024);
    if (video_frame == NULL)
    {
        LOGE("%s %d video_frame malloc fail\r\n", __func__, __LINE__);
        msg_send_rsp_to_media_major_mailbox(msg, BK_FAIL, APP_MODULE);
        return;
    }

    avi = AVI_open_input_file("/genie_eye.avi", 1);
    if (avi != NULL)
    {
        video_num = AVI_video_frames(avi);
        frame_size = avi->width * avi->height * 2;
        LOGI("avi video_num: %d, width: %d, height: %d, frame_size: %d\r\n", video_num, avi->width, avi->height, frame_size);
    }
    else
    {
        LOGE("open avi fail\r\n");
        os_free(video_frame);
        msg_send_rsp_to_media_major_mailbox(msg, BK_FAIL, APP_MODULE);
        return;
    }

    framebuffer = psram_malloc(frame_size);
    if (framebuffer == NULL)
    {
        LOGE("%s %d framebuffer malloc fail\r\n", __func__, __LINE__);
        AVI_close(avi);
        os_free(video_frame);
        msg_send_rsp_to_media_major_mailbox(msg, BK_FAIL, APP_MODULE);
        return;
    }

    segmentbuffer = psram_malloc(frame_size);
    if (segmentbuffer == NULL)
    {
        LOGE("%s %d framebuffer malloc fail\r\n", __func__, __LINE__);
        AVI_close(avi);
        os_free(video_frame);
        os_free(framebuffer);
        msg_send_rsp_to_media_major_mailbox(msg, BK_FAIL, APP_MODULE);
        return;
    }

    avi_video_frame_parse_to_rgb565(avi, pos, (uint8_t *)video_frame, (uint8_t *)framebuffer, video_len, frame_size);

    for (int i = 0; i < avi->height; i++)
    {
        os_memcpy(segmentbuffer + i * (avi->width >> 1), framebuffer + i * avi->width, avi->width);
        os_memcpy(segmentbuffer + (avi->width >> 1) * avi->height + i * (avi->width >> 1), framebuffer + i * avi->width + (avi->width >> 1), avi->width);
    }

    img_dsc.data = (const uint8_t *)segmentbuffer;

    lv_vendor_disp_lock();
    img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &img_dsc);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    lv_timer_t *timer = lv_timer_create(lv_timer_cb, 1000 / avi->fps, NULL);
    lv_timer_set_repeat_count(timer, -1);
    lv_vendor_disp_unlock();

    lv_vendor_start();

    msg_send_rsp_to_media_major_mailbox(msg, BK_OK, APP_MODULE);
}
#endif

#if (CONFIG_SYS_CPU0)
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

