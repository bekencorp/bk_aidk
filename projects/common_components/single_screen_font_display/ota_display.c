#include "os/os.h"
#include "os/mem.h"

#if (CONFIG_SYS_CPU1)
#include <driver/lcd.h>
#include "yuv_encode.h"
#include "bk_posix.h"
#include "lv_jpeg_hw_decode.h"
#include "modules/jpeg_decode_sw.h"
#include "lv_vendor.h"
#if CONFIG_LCD_QSPI
#include <lcd_qspi_display_service.h>
#endif
#include "media_evt.h"

static uint16_t *disp_frame = NULL;

lcd_open_t g_lcd_open =
{
    .device_ppi = PPI_360X360,
    .device_name = "gc9c01",
};

static bk_err_t bk_image_filelen_get(char *filename)
{
    bk_err_t ret = BK_FAIL;
    struct stat statbuf;

    do {
        if(!filename)
        {
            os_printf("[%s][%d] filename param is null.\r\n", __FUNCTION__, __LINE__);
            ret = BK_ERR_PARAM;
            break;
        }

        ret = stat(filename, &statbuf);
        if(BK_OK != ret)
        {
            os_printf("[%s][%d] sta fail:%s\r\n", __FUNCTION__, __LINE__, filename);
            break;
        }

        ret = statbuf.st_size;
        os_printf("[%s][%d] %s size:%d\r\n", __FUNCTION__, __LINE__, filename, ret);
    } while(0);

    return ret;
}

static bk_err_t bk_image_file_read(char *filename, void *read_buff, uint32_t data_len)
{
    int ret = BK_FAIL;
    int fd = -1;
    int read_len = 0;

    fd = open(filename, O_RDONLY);
    if(fd < 0)
    {
        os_printf("[%s][%d] %s open fail\r\n", __FUNCTION__, __LINE__, filename);
        return BK_FAIL;
    }

    read_len = read(fd, read_buff, data_len);
    if (read_len < 0)
    {
        os_printf("[%s][%d] read file fail.\r\n", __FUNCTION__, __LINE__);
        return BK_FAIL;
    }

    ret = close(fd);
    if (ret < 0)
    {
        os_printf("[%s][%d] close file fail.\r\n", __FUNCTION__, __LINE__);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t bk_ota_image_disp_open(char *filename)
{
    bk_err_t ret = BK_OK;
    uint8_t *jpeg_frame = NULL;
    uint32_t file_len = 0;
    sw_jpeg_dec_res_t result;

    lv_vendor_fs_init();

    bk_jpeg_hw_decode_to_mem_init();

    lcd_display_open(&g_lcd_open);

    file_len = bk_image_filelen_get(filename);
    if (file_len <= 0)
    {
        os_printf("[%s][%d] image file is not exist\r\n", __FUNCTION__, __LINE__);
        return BK_FAIL;
    }

    os_printf("file_len = %d\r\n", file_len);

    jpeg_frame = psram_malloc(file_len);
    if (jpeg_frame == NULL)
    {
        os_printf("[%s][%d] jpeg_frame malloc fail\r\n", __FUNCTION__, __LINE__);
        return BK_FAIL;
    }

    ret = bk_image_file_read(filename, jpeg_frame, file_len);
    if (ret != BK_OK)
    {
        os_printf("[%s][%d] image file read fail\r\n", __FUNCTION__, __LINE__);
        return BK_FAIL;
    }

    lv_vendor_fs_deinit();

    ret = bk_jpeg_get_img_info(file_len, jpeg_frame, &result);
    if (ret != BK_OK)
    {
        os_printf("[%s][%d] get jpeg img info fail\r\n", __FUNCTION__, __LINE__);
        return BK_FAIL;
    }

    os_printf("x = %d, y = %d\r\n", result.pixel_x, result.pixel_y);

    disp_frame = psram_malloc(result.pixel_x * result.pixel_y * 2);
    if (disp_frame == NULL)
    {
        os_printf("[%s][%d] disp_frame malloc fail\r\n", __FUNCTION__, __LINE__);
        return BK_FAIL;
    }

    bk_jpeg_hw_decode_to_mem(jpeg_frame, (uint8_t *)disp_frame, file_len, result.pixel_x, result.pixel_y);

    uint16_t *buf16 = disp_frame;
    for (int k = 0; k < result.pixel_x * result.pixel_y; k++)
    {
        buf16[k] = ((buf16[k] & 0xff00) >> 8) | ((buf16[k] & 0x00ff) << 8);
    }

    psram_free(jpeg_frame);
    jpeg_frame = NULL;

    bk_lcd_qspi_display((uint32_t)disp_frame);

    return BK_OK;
}

bk_err_t bk_ota_image_disp_close(void)
{
    lcd_display_close();

    bk_jpeg_hw_decode_to_mem_deinit();

    if (disp_frame)
    {
        psram_free(disp_frame);
        disp_frame = NULL;
    }

    return BK_OK;
}

#if CONFIG_OTA_DISPLAY_PICTURE_DEMO
void media_ui_ota_event_handle(media_mailbox_msg_t *msg)
{
	int ret = BK_FAIL;

	switch(msg->event)
	{
		case EVENT_OTA_DISP_OPEN_IND:
			if(bk_ota_image_disp_open("/ota_image.jpg") == BK_OK)
			{
				ret = BK_OK;
			}
			else
			{
				ret = BK_FAIL;
			}
			break;

		case EVENT_OTA_DISP_CLOSE_IND:
			bk_ota_image_disp_close();
			ret = BK_OK;
			break;

		default :
			break;
	}
	msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
}
#endif
#endif

