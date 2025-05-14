#include <os/os.h>
#include <os/str.h>
#include "lcd_act.h"
#include "media_app.h"
#if CONFIG_LVGL
    #include "lv_vendor.h"
    #include "lvgl.h"
    #include "lv_img_utility.h"
    #include "bk_posix.h"
#endif
#include "driver/drv_tp.h"
#include <driver/lcd.h>
#include "yuv_encode.h"
#include "media_evt.h"


#define TAG "AVI"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#if (CONFIG_SYS_CPU0)
    __attribute__((weak)) uint8_t lvgl_app_init_flag;
#endif

#if (CONFIG_SYS_CPU1)
__attribute__((weak)) void lvgl_event_handle(media_mailbox_msg_t *msg)
{

}

#endif
