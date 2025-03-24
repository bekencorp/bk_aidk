#pragma once

#include "components/log.h"

#include "stdint.h"

enum
{
    APP_MEDIA_DEBUG_LEVEL_ERROR,
    APP_MEDIA_DEBUG_LEVEL_WARNING,
    APP_MEDIA_DEBUG_LEVEL_INFO,
    APP_MEDIA_DEBUG_LEVEL_DEBUG,
    APP_MEDIA_DEBUG_LEVEL_VERBOSE,
};

#define APP_MEDIA_DEBUG_LEVEL APP_MEDIA_DEBUG_LEVEL_INFO

#define appm_loge(format, ...) do{if(APP_MEDIA_DEBUG_LEVEL >= APP_MEDIA_DEBUG_LEVEL_ERROR)   BK_LOGE("app_media", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define appm_logw(format, ...) do{if(APP_MEDIA_DEBUG_LEVEL >= APP_MEDIA_DEBUG_LEVEL_WARNING) BK_LOGW("app_media", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define appm_logi(format, ...) do{if(APP_MEDIA_DEBUG_LEVEL >= APP_MEDIA_DEBUG_LEVEL_INFO)    BK_LOGI("app_media", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define appm_logd(format, ...) do{if(APP_MEDIA_DEBUG_LEVEL >= APP_MEDIA_DEBUG_LEVEL_DEBUG)   BK_LOGD("app_media", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define appm_logv(format, ...) do{if(APP_MEDIA_DEBUG_LEVEL >= APP_MEDIA_DEBUG_LEVEL_VERBOSE) BK_LOGV("app_media", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)


#define CAMERA_TYPE DVP_CAMERA//DVP_CAMERA //UVC_CAMERA
#define PIXEL_WIDTH 640 //864
#define PIXEL_HEIGHT 480
#define CAMERA_FPS FPS30
#define ENABLE_LCD_SHOW_CAMERA 0
#define ENABLE_LVGL 0
#define ENABLE_TS_PRASE 1


int32_t media_main(void);
int32_t media_ts_main(void);
