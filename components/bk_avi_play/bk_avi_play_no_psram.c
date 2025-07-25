#include <os/os.h>
#include <os/mem.h>
#include "bk_avi_play.h"
#include "modules/jpeg_decode_sw.h"
#include "driver/lcd_types.h"
#include "yuv_encode.h"
#include "mux_pipeline.h"
#if CONFIG_LCD_QSPI
#include <driver/lcd_qspi.h>
#endif
#if CONFIG_LCD_SPI
#include <driver/lcd_spi.h>
#endif
#include "bk_posix.h"
#include "driver/flash_partition.h"
#if CONFIG_LITTLEFS_USE_LITTLEFS_PARTITION
#include "vendor_flash_partition.h"
#endif

#define TAG "avi_play"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static bk_avi_play_t *bk_avi_play = NULL;
static beken_timer_t g_avi_play_timer;


#if (CONFIG_FATFS)
static int _fs_mount(void)
{
    struct bk_fatfs_partition partition;
    char *fs_name = NULL;
    int ret;

    fs_name = "fatfs";
    partition.part_type = FATFS_DEVICE;
#if (CONFIG_SDCARD)
    partition.part_dev.device_name = FATFS_DEV_SDCARD;
#else
    partition.part_dev.device_name = FATFS_DEV_FLASH;
#endif
    partition.mount_path = "/";

    ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

    return ret;
}
#endif

#if (CONFIG_LITTLEFS)
static int _fs_mount(void)
{
    int ret;

    struct bk_little_fs_partition partition;
    char *fs_name = NULL;
#ifdef BK_PARTITION_LITTLEFS_USER
    bk_logic_partition_t *pt = bk_flash_partition_get_info(BK_PARTITION_LITTLEFS_USER);
#else
    bk_logic_partition_t *pt = bk_flash_partition_get_info(BK_PARTITION_USR_CONFIG);
#endif

    fs_name = "littlefs";
    partition.part_type = LFS_FLASH;
    partition.part_flash.start_addr = pt->partition_start_addr;
    partition.part_flash.size = pt->partition_length;
    partition.mount_path = "/";

    ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

    return ret;
}
#endif

bk_err_t bk_avi_play_vfs_init(void)
{
    bk_err_t ret = BK_FAIL;

    do {
        ret = _fs_mount();
        if (BK_OK != ret)
        {
            LOGE("[%s][%d] mount fail:%d\r\n", __FUNCTION__, __LINE__, ret);
            break;
        }

        LOGI("[%s][%d] mount success\r\n", __FUNCTION__, __LINE__);
    } while(0);

    return ret;
}

bk_err_t bk_avi_play_vfs_deinit(void)
{
    bk_err_t ret = BK_FAIL;

    ret = umount("/");
    if (BK_OK != ret) {
        LOGE("[%s][%d] unmount fail:%d\r\n", __FUNCTION__, __LINE__, ret);
    }

    LOGI("[%s][%d] unmount success\r\n", __FUNCTION__, __LINE__);

    return ret;
}

static void bk_jpeg_hw_decode_by_line_init(void)
{
    bk_err_t ret = BK_FAIL;

    rot_open_t rot_open = {0};
    rot_open.mode = SW_ROTATE;
    rot_open.fmt = PIXEL_FMT_RGB888;
    ret = rotate_task_open(&rot_open);
    if (ret != BK_OK)
    {
        LOGE("%s %d rotate_task_open fail\n", __func__, __LINE__);
    }

    ret = jpeg_decode_task_open(JPEGDEC_HW_MODE, JPEGDEC_BY_LINE, ROTATE_NONE);
    if (ret != BK_OK)
    {
        LOGE("%s, jpeg_decode_task_open fail\n", __func__);
    }

    bk_jdec_buffer_request_register(PIPELINE_MOD_ROTATE, bk_rotate_encode_request, bk_rotate_reset_request);
}

static void bk_jpeg_hw_decode_by_line_deinit(void)
{
    bk_err_t ret = BK_FAIL;

    bk_jdec_buffer_request_deregister(PIPELINE_MOD_ROTATE);
    ret = rotate_task_close();
    if (ret != BK_OK)
    {
        LOGE("%s %d rotate task close fail\n", __func__, __LINE__);
    }

    ret = jpeg_decode_task_close();
    if (ret != BK_OK)
    {
        LOGE("%s %d decode task close fail\n", __func__, __LINE__);
    }
}

static bk_err_t bk_jpeg_display_cb(void *param)
{
    lcd_partial_area_t *partial_area = (lcd_partial_area_t *)param;
    complex_buffer_t *buffer = (complex_buffer_t *)partial_area->buffer;
    uint8_t line = partial_area->height;
    if (partial_area->lcd_device->type == LCD_TYPE_SPI)
    {
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
        if (bk_avi_play->segment_flag == true)
        {
            bk_avi_play->framebuffer = (uint16_t *)buffer->data;
            for (int i = 0; i < line; i++)
            {
                os_memcpy(bk_avi_play->segmentbuffer + i * (bk_avi_play->avi->width >> 1), bk_avi_play->framebuffer + i * bk_avi_play->avi->width, bk_avi_play->avi->width);
                os_memcpy(bk_avi_play->segmentbuffer + (bk_avi_play->avi->width >> 1) * line + i * (bk_avi_play->avi->width >> 1), bk_avi_play->framebuffer + i * bk_avi_play->avi->width + (bk_avi_play->avi->width >> 1), bk_avi_play->avi->width);
            }
            lcd_spi_set_display_area(LCD_SPI_ID0, 0, partial_area->end_x - 1, partial_area->start_y, partial_area->end_y - 1);
            lcd_spi_partial_display(LCD_SPI_ID0, (uint8_t *)bk_avi_play->segmentbuffer, (bk_avi_play->avi->width >> 1) * line * 2);
            lcd_spi_set_display_area(LCD_SPI_ID1, 0, partial_area->end_x - 1, partial_area->start_y, partial_area->end_y - 1);
            lcd_spi_partial_display(LCD_SPI_ID1, (uint8_t *)(bk_avi_play->segmentbuffer + (bk_avi_play->avi->width >> 1) * line), (bk_avi_play->avi->width >> 1) * line * 2);
        }
        else
        {
            lcd_spi_set_display_area(LCD_SPI_ID0, 0, partial_area->end_x - 1, partial_area->start_y, partial_area->end_y - 1);
            lcd_spi_set_display_area(LCD_SPI_ID1, 0, partial_area->end_x - 1, partial_area->start_y, partial_area->end_y - 1);
            for (int i = 0; i < line; i++)
            {
                lcd_spi_partial_display(LCD_SPI_ID0, buffer->data + i * bk_avi_play->avi->width * 2, (bk_avi_play->avi->width >> 1) * 2);
                lcd_spi_partial_display(LCD_SPI_ID1, buffer->data + i * bk_avi_play->avi->width * 2 + (bk_avi_play->avi->width >> 1) * 2, (bk_avi_play->avi->width >> 1) * 2);
            }
        }
#else
        lcd_spi_set_display_area(LCD_SPI_ID, 0, partial_area->end_x - 1, partial_area->start_y, partial_area->end_y - 1);
        lcd_spi_partial_display(LCD_SPI_ID, buffer->data, bk_avi_play->avi->width * bk_avi_play->avi->height * 2);
#endif
    }
    else if (partial_area->lcd_device->type == LCD_TYPE_QSPI)
    {
        // lcd_qspi_partial_display(QSPI_ID_0, partial_area->lcd_device,
        //                         partial_area->start_x, partial_area->end_x - 1, 
        //                         partial_area->start_y, partial_area->end_y - 1, buffer->data);
    }
    else
    {
        lcd_display_partial_refresh(partial_area);
    }

    partial_area->post_refresh(partial_area);

    return BK_OK;
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
    ret = bk_jpeg_hw_decode_by_line((uint8_t *)avi_play->video_frame, avi_play->video_len, avi_play->avi->width, avi_play->avi->height);
    if (ret != BK_OK)
    {
        LOGE("%s %d bk_jpeg_hw_decode_to_mem failed\r\n", __func__, __LINE__);
        return ret;
    }
#else
    // sw_jpeg_dec_res_t result;
    // bk_jpeg_dec_sw_start(JPEGDEC_BY_FRAME, (uint8_t *)avi_play->video_frame, (uint8_t *)avi_play->framebuffer, avi_play->video_len, avi_play->frame_size, (sw_jpeg_dec_res_t *)&result);
#endif

    return ret;
}

static void bk_avi_play_config_free(bk_avi_play_t *avi_play)
{
    if (avi_play->avi)
    {
        AVI_close(avi_play->avi);
        avi_play->avi = NULL;
    }

    if (avi_play->video_frame)
    {
        os_free(avi_play->video_frame);
        avi_play->video_frame = NULL;
    }

    if (avi_play->framebuffer)
    {
        os_free(avi_play->framebuffer);
        avi_play->framebuffer = NULL;
    }

    if (avi_play->segmentbuffer)
    {
        os_free(avi_play->segmentbuffer);
        avi_play->segmentbuffer = NULL;
    }
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

    bk_avi_play_vfs_init();

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

    bk_avi_play->video_frame = os_malloc(AVI_VIDEO_MAX_FRAME_LEN);
    if (bk_avi_play->video_frame == NULL)
    {
        LOGE("%s video_frame malloc fail\r\n", __func__);
        goto out;
    }

    if (bk_avi_play->segment_flag == true)
    {
        bk_avi_play->segmentbuffer = os_malloc(bk_avi_play->avi->width * 16 * 2);
        if (bk_avi_play->segmentbuffer == NULL)
        {
            LOGE("%s %d segmentbuffer malloc fail\r\n", __func__, __LINE__);
            goto out;
        }
    }

#if AVI_VIDEO_USE_HW_DECODE
    bk_jpeg_hw_decode_by_line_init();
    bk_jdec_display_request_register(bk_jpeg_display_cb);
#else
    // jd_output_format format = {0};

    // bk_jpeg_dec_sw_init(NULL, 0);
    // format.format = JD_FORMAT_RGB565;
    // format.scale = 0;
    // if (bk_avi_play->swap_flag == true)
    // {
    //     format.byte_order = JD_BIG_ENDIAN;
    // }
    // else
    // {
    //     format.byte_order = JD_LITTLE_ENDIAN;
    // }
    // jd_set_output_format(&format);
#endif

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

    bk_avi_play_vfs_deinit();

    return BK_FAIL;
}

void bk_avi_play_close(void)
{
#if AVI_VIDEO_USE_HW_DECODE
    bk_jpeg_hw_decode_by_line_deinit();
#else
    // bk_jpeg_dec_sw_deinit();
#endif

    bk_avi_play_config_free(bk_avi_play);

    if (bk_avi_play)
    {
        os_free(bk_avi_play);
        bk_avi_play = NULL;
    }

    bk_avi_play_vfs_deinit();

    LOGI("%s complete\r\n", __func__);
}

static void bk_avi_timer_cb(void *param)
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
}

void bk_avi_play_start(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_timer(&g_avi_play_timer, 1000 / bk_avi_play->avi->fps, bk_avi_timer_cb, NULL);
    if (ret != kNoErr) {
        LOGE("%s g_avi_play_timer init failed\r\n", __func__);
        return;
    }

    ret = rtos_start_timer(&g_avi_play_timer);
    if (ret != kNoErr) {
        LOGE("%s g_avi_play_timer start failed\r\n", __func__);
        rtos_deinit_timer(&g_avi_play_timer);
        return;
    }

    LOGI("%s complete\r\n", __func__);
}

void bk_avi_play_stop(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_stop_timer(&g_avi_play_timer);
    if (ret != kNoErr) {
        LOGE("%s g_avi_play_timer stop failed\r\n", __func__);
        return;
    }

    ret = rtos_deinit_timer(&g_avi_play_timer);
    if (ret != kNoErr) {
        LOGE("%s g_avi_play_timer stop failed\r\n", __func__);
        return;
    }

    LOGI("%s complete\r\n", __func__);
}

