#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <driver/audio_ring_buff.h>
#include "video_engine.h"
#include "video_log.h"
#include <modules/wifi.h>
#include "modules/wifi_types.h"

bk_err_t voide_engine_init(void)
{
    #if (CONFIG_SYS_CPU1 && CONFIG_IMAGE_DEBUG_DUMP)
    video_dump_data_cli_init();
    #endif
    return BK_OK;
}


#if CONFIG_SYS_CPU0

static user_video_tx_data_func video_tx_func = NULL;
bool video_started = false;

static media_camera_device_t camera_device =
{
#if (CONFIG_VIDEO_ENGINE_USE_UVC_CAMERA)
    .type = UVC_CAMERA,
#if (CONFIG_VIDEO_ENGINE_JPEG_FORMAT)
    .mode = JPEG_MODE,
    .fmt  = PIXEL_FMT_JPEG,
#endif
#if (CONFIG_VIDEO_ENGINE_H264_FORMAT)
    .mode = H264_MODE,
    .fmt  = PIXEL_FMT_H264,
#endif
    /* expect the width and length */
    .info.resolution.width  = CONFIG_VIDEO_ENGINE_RESOLUTION_WIDTH,//640,//864,
    .info.resolution.height = CONFIG_VIDEO_ENGINE_RESOLUTION_HEIGHT,
    .info.fps = FPS25,
#elif (CONFIG_VIDEO_ENGINE_USE_DVP_CAMERA)
    /* DVP Camera */ 
    .type = DVP_CAMERA,
#if (CONFIG_VIDEO_ENGINE_JPEG_FORMAT)
    .mode = JPEG_MODE,
    .fmt  = PIXEL_FMT_JPEG,
#endif
#if (CONFIG_VIDEO_ENGINE_H264_FORMAT)
    .mode = H264_MODE,
    .fmt  = PIXEL_FMT_H264,
#endif
    /* expect the width and length */
    .info.resolution.width  = CONFIG_VIDEO_ENGINE_RESOLUTION_WIDTH,
    .info.resolution.height = CONFIG_VIDEO_ENGINE_RESOLUTION_HEIGHT,
    .info.fps = FPS20,
#endif
};

void video_register_tx_data_func(user_video_tx_data_func func)
{
    video_tx_func = func;
}

#if (CONFIG_VIDEO_ENGINE_USE_UVC_CAMERA)
static void media_checkout_uvc_device_info(bk_uvc_device_brief_info_t *info, uvc_state_t state)
{
    bk_uvc_config_t uvc_config_info_param = {0};
    uint8_t format_index = 0;
    uint8_t frame_num = 0;
    uint8_t index = 0;

    if (state == UVC_CONNECTED)
    {
        uvc_config_info_param.vendor_id  = info->vendor_id;
        uvc_config_info_param.product_id = info->product_id;

        format_index = info->format_index.mjpeg_format_index;
        frame_num    = info->all_frame.mjpeg_frame_num;
        if (format_index > 0)
        {
            LOGI("%s uvc_get_param MJPEG format_index:%d\r\n", __func__, format_index);
            for (index = 0; index < frame_num; index++)
            {
                LOGI("uvc_get_param MJPEG width:%d heigth:%d index:%d\r\n",
                     info->all_frame.mjpeg_frame[index].width,
                     info->all_frame.mjpeg_frame[index].height,
                     info->all_frame.mjpeg_frame[index].index);
                for (int i = 0; i < info->all_frame.mjpeg_frame[index].fps_num; i++)
                {
                    LOGI("uvc_get_param MJPEG fps:%d\r\n", info->all_frame.mjpeg_frame[index].fps[i]);
                }
    
                if (info->all_frame.mjpeg_frame[index].width == camera_device.info.resolution.width
                    && info->all_frame.mjpeg_frame[index].height == camera_device.info.resolution.height)
                {
                    uvc_config_info_param.frame_index = info->all_frame.mjpeg_frame[index].index;
                    uvc_config_info_param.fps         = info->all_frame.mjpeg_frame[index].fps[0];
                    uvc_config_info_param.width       = camera_device.info.resolution.width;
                    uvc_config_info_param.height      = camera_device.info.resolution.height;
                }
            }
        }

        uvc_config_info_param.format_index = format_index;

        if (media_app_set_uvc_device_param(&uvc_config_info_param) != BK_OK)
        {
            LOGE("%s, failed\r\n, __func__");
        }
    }
    else
    {
        LOGI("%s, %d\r\n", __func__, state);
    }
}
#endif
bk_err_t video_turn_off(void)
{
    bk_err_t ret =  BK_OK;
    VDOE_LOGI("%s\n", __func__);

    if (!video_started) {
        VDOE_LOGW("%s already turn off\n", __func__);
        return BK_OK;
    }	
    ret = media_app_unregister_read_frame_callback();
    if (ret != BK_OK)
    {
        VDOE_LOGE("%s, %d, unregister read_frame_cb failed\n", __func__, __LINE__);
    }

    ret = media_app_h264_pipeline_close();
    if (ret != BK_OK)
    {
        VDOE_LOGE("%s, %d, h264_pipeline_close failed\n", __func__, __LINE__);
    }

    ret = media_app_camera_close(camera_device.type);
    if (ret != BK_OK)
    {
        VDOE_LOGE("%s, %d, media_app_camera_close failed\n", __func__, __LINE__);
    }

    bk_wifi_set_wifi_media_mode(false);
    bk_wifi_set_video_quality(WIFI_VIDEO_QUALITY_HD);
    video_started = false;

    return BK_OK;
}

bk_err_t video_turn_on(void)
{
    bk_err_t ret = BK_OK;
    VDOE_LOGI("%s\n", __func__);

    if (video_started) {
        VDOE_LOGW("%s already turn on\n", __func__);
        return BK_OK;
    }
    bk_wifi_set_wifi_media_mode(true);
    bk_wifi_set_video_quality(WIFI_VIDEO_QUALITY_FD);

#if (CONFIG_VIDEO_ENGINE_USE_UVC_CAMERA)
    media_app_uvc_register_info_notify_cb(media_checkout_uvc_device_info);
#endif

    ret = media_app_camera_open(&camera_device);
    if (ret != BK_OK)
    {
        VDOE_LOGE("%s, %d, media_app_camera_open failed, ret: %d\n", __func__, __LINE__, ret);
        goto fail;
    }

    if (video_tx_func == NULL)
    {
        VDOE_LOGE("%s, %d, video_tx_func is NULL\n", __func__, __LINE__);
        goto fail;
    }

    bool media_mode = false;
    uint8_t quality = 0;
    bk_wifi_get_wifi_media_mode_config(&media_mode);
    bk_wifi_get_video_quality_config(&quality);
    VDOE_LOGE("~~~~~~~~~~wifi media mode %d, video quality %d~~~~~~\r\n", media_mode, quality);

#if (CONFIG_VIDEO_ENGINE_USE_UVC_CAMERA)
    ret = media_app_h264_pipeline_open();
    if (ret != BK_OK)
    {
        VDOE_LOGE("%s, %d, h264_pipeline_open failed, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }

    ret = media_app_register_read_frame_callback(PIXEL_FMT_H264, video_tx_func);
    if (ret != BK_OK)
    {
        VDOE_LOGE("%s, %d, register read_frame_cb failed, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }
#elif (CONFIG_VIDEO_ENGINE_USE_DVP_CAMERA)
    ret = media_app_register_read_frame_callback(camera_device.fmt, video_tx_func);
    if (ret != BK_OK)
    {
        VDOE_LOGE("%s, %d, register read_frame_cb failed, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }
#endif
    video_started = true;

    return BK_OK;

fail:
    video_turn_off();

    return BK_FAIL;
}
#endif