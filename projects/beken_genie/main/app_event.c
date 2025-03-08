#include <common/sys_config.h>
#include <components/log.h>
#include <modules/wifi.h>
#include <components/netif.h>
#include <components/event.h>
#include <string.h>

#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>

#include "app_event.h"
#include "media_app.h"
#include "led_blink.h"

#include "countdown.h"


#define TAG "app_evt"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


typedef struct
{
    beken_thread_t thread;
    beken_queue_t queue;
} app_evt_info_t;

typedef struct
{
    uint32_t event;
    uint32_t param;
} app_evt_msg_t;

extern void lvgl_app_init(void);
extern void lvgl_app_deinit(void);


static app_evt_info_t app_evt_info;




bk_err_t app_event_send_msg(uint32_t event, uint32_t param)
{
    bk_err_t ret;
    app_evt_msg_t msg;

    msg.event = event;
    msg.param = param;

    ret = rtos_push_to_queue(&app_evt_info.queue, &msg, BEKEN_NO_WAIT);
    if (BK_OK != ret)
    {
        LOGE("%s, %d : %d fail \n", __func__, __LINE__, event);
        return BK_FAIL;
    }

    return BK_FAIL;
}

void app_event_asr_evt_callback(media_app_evt_type_t event, uint32_t param)
{
    LOGD("asr event callback: %x\n", event);

    /*Do not do anything blocking here */

    switch (event)
    {
        case MEDIA_APP_EVT_ASR_WAKEUP_IND:
            app_event_send_msg(APP_EVT_ASR_WAKEUP, 0);
            break;
        case MEDIA_APP_EVT_ASR_STANDBY_IND:
            app_event_send_msg(APP_EVT_ASR_STANDBY, 0);
            break;
    }
}


static void app_event_thread(beken_thread_arg_t data)
{
    int ret = BK_OK;

    media_app_asr_evt_register_callback(app_event_asr_evt_callback);

    while (1)
    {
        app_evt_msg_t msg;

        ret = rtos_pop_from_queue(&app_evt_info.queue, &msg, BEKEN_WAIT_FOREVER);

        if (ret == BK_OK)
        {
            switch (msg.event)
            {
                case APP_EVT_ASR_WAKEUP:
                    LOGI("APP_EVT_ASR_WAKEUP\n");
                    lvgl_app_init();
                    stop_countdown();
                    led_app_set(LED_OFF_GREEN);
                    break;
                case APP_EVT_ASR_STANDBY:
                    LOGI("APP_EVT_ASR_STANDBY\n");
                    lvgl_app_deinit();
                    start_countdown();
                    break;
                case APP_EVT_PAIRING_NETWORK:
                    LOGI("APP_EVT_PAIRING_NETWORK\n");
                    led_app_set(LED_REG_GREEN_ALTERNATE);
                    break;
                case APP_EVT_RECONNECT_NETWORK:
                    LOGI("APP_EVT_RECONNECT_NETWORK\n");
			led_app_set(LED_OFF_RED);
                    led_app_set(LED_FAST_BLINK_GREEN);
                    break;
                case APP_EVT_CONNECT_NETWORK_FAIL:
                    LOGI("APP_EVT_CONNECT_NETWORK_FAIL\n");
			led_app_set(LED_OFF_GREEN);
                    led_app_set(LED_FAST_BLINK_RED);
                    break;
                case APP_EVT_RTC_CONNECTION_LOST:
                    LOGI("APP_EVT_RTC_CONNECTION_LOST\n");
			led_app_set(LED_OFF_GREEN);
                    led_app_set(LED_FAST_BLINK_RED);
                    break;
                case APP_EVT_AGENT_JOINED:
                    LOGI("APP_EVT_AGENT_JOINED\n");
			led_app_set(LED_OFF_RED);
                    led_app_set(LED_SLOW_BLINK_GREEN);
                    break;
                case APP_EVT_AGENT_OFFLINE:
                    LOGI("APP_EVT_AGENT_OFFLINE\n");
			led_app_set(LED_OFF_GREEN);
                    led_app_set(LED_FAST_BLINK_RED);
                    break;
                case APP_EVT_LOW_VOLTAGE:
                    LOGI("APP_EVT_LOW_VOLTAGE\n");
                    led_app_set(LED_SLOW_BLINK_RED);
                    break;
                default:
                    break;
            }
        }
    }

    LOGI("%s, exit\r\n", __func__);
    rtos_delete_thread(NULL);
}



void app_event_init(void)
{
    int ret = BK_FAIL;

    os_memset(&app_evt_info, 0, sizeof(app_evt_info_t));

    ret = rtos_init_queue(&app_evt_info.queue,
                          "ae_queue",
                          sizeof(app_evt_msg_t),
                          15);

    if (ret != BK_OK)
    {
        LOGE("%s, init queue failed\r\n", __func__);
        return;
    }

    ret = rtos_create_thread(&app_evt_info.thread,
                             BEKEN_DEFAULT_WORKER_PRIORITY - 1,
                             "ae_thread",
                             (beken_thread_function_t)app_event_thread,
                             1024 * 4,
                             NULL);

    if (ret != BK_OK)
    {
        LOGE("%s, init thread failed\r\n", __func__);
        return;
    }
}
