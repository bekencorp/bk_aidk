#include <os/os.h>
#include "bk_avi_play.h"
#include "modules/jpeg_decode_sw.h"
#include "lv_jpeg_hw_decode.h"

#if CONFIG_LVGL
#include "lv_vendor.h"
#include "lvgl.h"
#endif

#define TAG "avi_play"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


static bk_avi_play_t *bk_avi_play = NULL;
static lv_obj_t *avi_img = NULL;
static lv_timer_t *avi_timer = NULL;
static lv_img_dsc_t avi_img_dsc =
{
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .data_size = 0,
    .data = NULL,
};

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

    if (avi_play->swap_flag == true) {
        uint16_t *buf16 = (uint16_t *)avi_play->framebuffer;
        for (int k = 0; k < avi_play->avi->width * avi_play->avi->height; k++)
        {
            buf16[k] = ((buf16[k] & 0xff00) >> 8) | ((buf16[k] & 0x00ff) << 8);
        }
    }
#else
    sw_jpeg_dec_res_t result;
    bk_jpeg_dec_sw_start(JPEGDEC_BY_FRAME, (uint8_t *)avi_play->video_frame, (uint8_t *)avi_play->framebuffer, avi_play->video_len, avi_play->frame_size, (sw_jpeg_dec_res_t *)&result);
#endif

    if (avi_play->segment_flag == true)
    {
        for (int i = 0; i < avi_play->avi->height; i++)
        {
            os_memcpy(avi_play->segmentbuffer + i * (avi_play->avi->width >> 1), avi_play->framebuffer + i * avi_play->avi->width, avi_play->avi->width);
            os_memcpy(avi_play->segmentbuffer + (avi_play->avi->width >> 1) * avi_play->avi->height + i * (avi_play->avi->width >> 1), avi_play->framebuffer + i * avi_play->avi->width + (avi_play->avi->width >> 1), avi_play->avi->width);
        }
    }

    return ret;
}

bk_err_t bk_avi_play_open(const char *filename, bool segment_flag, bool color_swap)
{
    bk_avi_play = os_malloc(sizeof(bk_avi_play_t));
    if (bk_avi_play == NULL)
    {
        LOGE("%s %d bk_avi_play malloc failed\r\n", __func__, __LINE__);
        return BK_FAIL;
    }
    os_memset(bk_avi_play, 0x00, sizeof(bk_avi_play_t));

    lv_vendor_fs_init();

    bk_avi_play->avi = AVI_open_input_file(filename, 1);
    if (bk_avi_play->avi == NULL)
    {
        LOGE("%s open avi file failed\r\n", __func__);
        goto out;
    }
    else
    {
        bk_avi_play->video_num = AVI_video_frames(bk_avi_play->avi);
        bk_avi_play->frame_size = bk_avi_play->avi->width * bk_avi_play->avi->height * 2;
        LOGI("avi video_num: %d, width: %d, height: %d, frame_size: %d, fps: %d\r\n", bk_avi_play->video_num, bk_avi_play->avi->width, bk_avi_play->avi->height, bk_avi_play->frame_size, (uint32_t)bk_avi_play->avi->fps);
    }

    bk_avi_play->pos = 0;
    bk_avi_play->segment_flag = segment_flag;
    bk_avi_play->swap_flag = color_swap;

    bk_avi_play->video_frame = psram_malloc(AVI_VIDEO_MAX_FRAME_LEN);
    if (bk_avi_play->video_frame == NULL)
    {
        LOGE("%s video_frame malloc fail\r\n", __func__);
        goto out;
    }

    bk_avi_play->framebuffer = psram_malloc(bk_avi_play->frame_size);
    if (bk_avi_play->framebuffer == NULL)
    {
        LOGE("%s framebuffer malloc fail\r\n", __func__);
        goto out;
    }

    if (bk_avi_play->segment_flag == true)
    {
        bk_avi_play->segmentbuffer = psram_malloc(bk_avi_play->frame_size);
        if (bk_avi_play->segmentbuffer == NULL)
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
    if (bk_avi_play->swap_flag == true)
    {
        format.byte_order = JD_BIG_ENDIAN;
    }
    else
    {
        format.byte_order = JD_LITTLE_ENDIAN;
    }
    jd_set_output_format(&format);
#endif

#if (CONFIG_LCD_SPI_DEVICE_NUM > 1 || CONFIG_LCD_QSPI_DEVICE_NUM > 1)
    avi_img_dsc.header.w = bk_avi_play->avi->height;
    avi_img_dsc.header.h = bk_avi_play->avi->width;
    avi_img_dsc.data_size = avi_img_dsc.header.w * avi_img_dsc.header.h * 2;
#else
    avi_img_dsc.header.w = bk_avi_play->avi->width;
    avi_img_dsc.header.h = bk_avi_play->avi->height;
    avi_img_dsc.data_size = avi_img_dsc.header.w * avi_img_dsc.header.h * 2;
#endif

    if (bk_avi_play->segment_flag == true) {
        avi_img_dsc.data = (const uint8_t *)bk_avi_play->segmentbuffer;
    } else {
        avi_img_dsc.data = (const uint8_t *)bk_avi_play->framebuffer;
    }

    bk_err_t ret = bk_avi_video_prase_to_rgb565(bk_avi_play);
    if (ret != BK_OK) {
        LOGE("%s %d bk_avi_video_prase_to_rgb565 failed\r\n", __func__, __LINE__);
        goto out;
    }

    LOGI("%s complete\r\n", __func__);

    return BK_OK;

out:
    bk_avi_play_config_free(bk_avi_play);

    if (bk_avi_play)
    {
        os_free(bk_avi_play);
        bk_avi_play = NULL;
    }

    lv_vendor_fs_deinit();

    return BK_FAIL;
}

void bk_avi_play_close(void)
{
#if AVI_VIDEO_USE_HW_DECODE
    bk_jpeg_hw_decode_to_mem_deinit();
#else
    bk_jpeg_dec_sw_deinit();
#endif

    bk_avi_play_config_free(bk_avi_play);

    if (bk_avi_play)
    {
        os_free(bk_avi_play);
        bk_avi_play = NULL;
    }

    lv_vendor_fs_deinit();

    LOGI("%s complete\r\n", __func__);
}

static void lv_timer_cb(lv_timer_t *timer)
{
    bk_err_t ret = BK_OK;

    bk_avi_play->pos++;
    if (bk_avi_play->pos >= bk_avi_play->video_num)
    {
        bk_avi_play->pos = 0;
    }

    ret = bk_avi_video_prase_to_rgb565(bk_avi_play);
    if (ret != BK_OK)
    {
        LOGE("%s %d bk_avi_video_prase_to_rgb565 failed\r\n", __func__, __LINE__);
        return;
    }

    lv_img_set_src(avi_img, &avi_img_dsc);
}

void bk_avi_play_start(void)
{
    avi_img = lv_img_create(lv_scr_act());
    lv_img_set_src(avi_img, &avi_img_dsc);
    lv_obj_align(avi_img, LV_ALIGN_CENTER, 0, 0);

    avi_timer = lv_timer_create(lv_timer_cb, 1000 / bk_avi_play->avi->fps, NULL);

    LOGI("%s complete\r\n", __func__);
}

void bk_avi_play_stop(void)
{
    lv_timer_del(avi_timer);
    avi_timer = NULL;

    lv_obj_del(avi_img);
    avi_img = NULL;

    LOGI("%s complete\r\n", __func__);
}

