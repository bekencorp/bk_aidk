#include <os/os.h>
#include "lcd_act.h"
#include "media_app.h"
#include "yuv_encode.h"
#include "media_evt.h"
#include "bk_avi_play.h"
#include "driver/lcd_spi.h"

#define TAG "avi_no_psram"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

bk_err_t bk_avi_play_event_start_handle(media_mailbox_msg_t *msg)
{
    bk_err_t ret = BK_OK;

    LOGI("%s \n", __func__);

    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;

    ret = lcd_display_open(lcd_open);
    if (ret != BK_OK) {
        LOGE("%s lcd_display_open failed\r\n", __func__);
        return ret;
    }

    ret = bk_avi_play_open("/genie_eye.avi", true, false);
    if (ret != BK_OK) {
        LOGE("%s bk_avi_play_open failed\r\n", __func__);
        lcd_display_close();
        return ret;
    }

    bk_avi_play_start();

    LOGI("%s is complete\n", __func__);

    return ret;
}

bk_err_t bk_avi_play_event_stop_handle(media_mailbox_msg_t *msg)
{
    bk_err_t ret = BK_OK;

    LOGI("%s \n", __func__);

    bk_avi_play_stop();

    lcd_display_close();

    bk_avi_play_close();

    LOGI("%s is complete\n", __func__);

    return ret;
}

void bk_avi_play_event_handle(media_mailbox_msg_t *msg)
{
    bk_err_t ret = BK_FAIL;

    switch (msg->event)
    {
        case EVENT_AVI_PLAY_START_IND:
            ret = bk_avi_play_event_start_handle(msg);
            break;

        case EVENT_AVI_PLAY_STOP_IND:
            ret = bk_avi_play_event_start_handle(msg);
            break;

        default:
            break;
    }

    msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
}