#include "media_main.h"
#include "driver/media_types.h"
#include "driver/lcd_types.h"
#include "media_app.h"
#include "gpio_driver.h"
#include "driver/gpio.h"

int32_t media_main(void)
{
    appm_logi("");
    bk_err_t ret = BK_FAIL;

    media_camera_device_t device = {0};

    if (CAMERA_TYPE == UVC_CAMERA)
    {
        device.type = UVC_CAMERA;

        //.port = 1,
        //        .format = IMAGE_MJPEG,
        //        .width = 864,
        //        .height = 480,
        //        .fps = FPS30,

        device.mode = JPEG_MODE;
        device.fmt = PIXEL_FMT_JPEG;
        device.info.fps = CAMERA_FPS;//FPS25,
        device.info.resolution.width = PIXEL_WIDTH;//864,
        device.info.resolution.height = PIXEL_HEIGHT;
    }
    else if (CAMERA_TYPE == DVP_CAMERA)
    {
        device.type = DVP_CAMERA;
        device.mode = YUV_MODE;//JPEG_YUV_MODE;
        device.fmt = PIXEL_FMT_JPEG;
        device.info.fps = 25;//CAMERA_FPS;
        device.info.resolution.width = PIXEL_WIDTH;
        device.info.resolution.height = PIXEL_HEIGHT;
    }

    appm_logi("try open camera type %d", device.type);

    ret = media_app_camera_open(&device);

    if (ret != BK_OK)
    {
        appm_loge("media_app_camera_open failed");
        return ret;
    }

    appm_logi("camera open success");

    //media_app_set_rotate(ROTATE_NONE);
    if (device.type == UVC_CAMERA)
    {
        media_app_pipline_set_rotate(ROTATE_90);
        appm_logi("set app rotate 1 success");
    }
    else
    {
    }

#if 0
    //ret = media_app_pipeline_h264_open();
    ret = media_app_h264_pipeline_open();

    if (ret != BK_OK)
    {
        appm_loge("h264_pipeline_open failed");
        return ret;
    }

#endif

#if ENABLE_LCD_SHOW_CAMERA

    if (device.type == UVC_CAMERA)
    {
        appm_logi("try open pipeline jdec");
        //media_app_pipeline_jdec_open();
        ret = media_app_lcd_pipeline_jdec_open();

        if (ret != BK_OK)
        {
            appm_loge("media_app_lcd_pipeline_jdec_open failed");
            return ret;
        }

        appm_logi("open pipeline jdec done");
    }

    uint32_t lcd_type_count = 1;//media_app_get_lcd_devices_num();
    const lcd_device_t **lcd_device_list = (typeof(lcd_device_list))media_app_get_lcd_devices_list();

    lcd_open_t lcd_open =
    {
        .device_ppi = PPI_640X480,//PPI_480X854, //PPI_640X480
        .device_name = "st7701sn",
    };


    //media_app_set_rotate(ROTATE_NONE);
    //media_app_pipline_set_rotate(ROTATE_NONE);

    //appm_logi("set app rotate 2 success");
    //    ret = media_app_lcd_disp_open((lcd_open_t *)&lcd_open);

    appm_logi("try open lcd");

    for (uint32_t i = 0; i < lcd_type_count; ++i)
    {
        if (device.type == UVC_CAMERA)
        {
            ret = media_app_lcd_pipeline_open((lcd_open_t *)&lcd_open);

            if (ret != BK_OK)
            {
                appm_loge("media_app_pipeline_jdec_disp_open failed");
                return -1;
            }
        }
        else if (device.type == DVP_CAMERA)
        {
            media_app_lcd_rotate(ROTATE_90);
            ret = media_app_lcd_open((lcd_open_t *)&lcd_open);

            if (ret != BK_OK)
            {
                appm_loge("media_app_lcd_open failed");
                return -1;
            }
        }
    }

    appm_logi("open lcd success");
#endif

#if ENABLE_LVGL
    appm_logi("try open lvgl");
    ret = media_app_lvgl_open((lcd_open_t *)&lcd_open);

    if (ret != BK_OK)
    {
        appm_loge("media_app_lvgl_draw failed");
        return -1;
    }

    appm_logi("open lvgl success");

#endif

    return 0;
}
