#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <components/log.h>

#include "app_evt_process.h"
#include "app_audio_arbiter.h"
#include "app_event.h"
#include "a2dp_sink_demo.h"

enum
{
    APP_DEBUG_LEVEL_ERROR,
    APP_DEBUG_LEVEL_WARNING,
    APP_DEBUG_LEVEL_INFO,
    APP_DEBUG_LEVEL_DEBUG,
    APP_DEBUG_LEVEL_VERBOSE,
};

#define APP_DEBUG_LEVEL APP_DEBUG_LEVEL_INFO
#define LOG_TAG "app_evt_process"
#define LOGE(format, ...) do{if(APP_DEBUG_LEVEL_INFO >= APP_DEBUG_LEVEL_ERROR)   BK_LOGE(LOG_TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define LOGW(format, ...) do{if(APP_DEBUG_LEVEL_INFO >= APP_DEBUG_LEVEL_WARNING) BK_LOGW(LOG_TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define LOGI(format, ...) do{if(APP_DEBUG_LEVEL_INFO >= APP_DEBUG_LEVEL_INFO)    BK_LOGI(LOG_TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define LOGD(format, ...) do{if(APP_DEBUG_LEVEL_INFO >= APP_DEBUG_LEVEL_DEBUG)   BK_LOGI(LOG_TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define LOGV(format, ...) do{if(APP_DEBUG_LEVEL_INFO >= APP_DEBUG_LEVEL_VERBOSE) BK_LOGI(LOG_TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)

static void ai_audio_source_entry_cb(AUDIO_SOURCE_ENTRY_CB_EVT evt, void *arg)
{
    LOGI("evt %d", evt);

    switch (evt)
    {
    case AUDIO_SOURCE_ENTRY_CB_EVT_NEED_START:
        break;

    case AUDIO_SOURCE_ENTRY_CB_EVT_IGNORE:
        break;

    case AUDIO_SOURCE_ENTRY_CB_EVT_NEED_STOP:
        break;

    default:
        break;
    }
}

static void user_audio_source_entry_cb(AUDIO_SOURCE_ENTRY_CB_EVT evt, void *arg)
{
    LOGI("evt %d", evt);

    switch (evt)
    {
    case AUDIO_SOURCE_ENTRY_CB_EVT_NEED_START:
        break;

    case AUDIO_SOURCE_ENTRY_CB_EVT_IGNORE:
        break;

    case AUDIO_SOURCE_ENTRY_CB_EVT_NEED_STOP:
        break;

    default:
        break;
    }
}

static void app_event_callback(app_evt_msg_t *msg, void *user_data)
{
    AUDIO_SOURCE_ENTRY_STATUS audio_source_arbiter_entry_status = AUDIO_SOURCE_ENTRY_STATUS_STOP;

    if (!msg)
    {
        LOGE("msg NULL !!!");
        return;
    }

    LOGI("msg %d", msg->event);

    switch (msg->event)
    {
    case APP_EVT_ASR_WAKEUP:
    {
        a2dp_sink_demo_vote_enable_leagcy(0);
        audio_source_arbiter_entry_status = app_audio_arbiter_report_source_req(AUDIO_SOURCE_ENTRY_AI, AUDIO_SOURCE_ENTRY_ACTION_START_REQ);

        if (audio_source_arbiter_entry_status != AUDIO_SOURCE_ENTRY_STATUS_PLAY)
        {
            LOGE("audio arbiter start status err %d", audio_source_arbiter_entry_status);
        }
    }
    break;

    case APP_EVT_ASR_STANDBY:
    {
        a2dp_sink_demo_vote_enable_leagcy(1);
        audio_source_arbiter_entry_status = app_audio_arbiter_report_source_req(AUDIO_SOURCE_ENTRY_AI, AUDIO_SOURCE_ENTRY_ACTION_STOP_REQ);

        if (audio_source_arbiter_entry_status != AUDIO_SOURCE_ENTRY_STATUS_STOP)
        {
            LOGI("audio arbiter stop status err %d", audio_source_arbiter_entry_status);
        }
    }
    break;

    default:
        break;
    }

    return;
}

void app_evt_process_init(void)
{
    int ret = 0;
    ret = app_event_register_handler(APP_EVT_ASR_WAKEUP, app_event_callback, NULL);

    if (ret)
    {
        LOGE("app_event_register_handler err %d", ret);
    }

    ret = app_event_register_handler(APP_EVT_ASR_STANDBY, app_event_callback, NULL);

    if (ret)
    {
        LOGE("app_event_register_handler err %d", ret);
    }

    app_audio_arbiter_reg_callback(AUDIO_SOURCE_ENTRY_AI, ai_audio_source_entry_cb, NULL);
    app_audio_arbiter_reg_callback(AUDIO_SOURCE_ENTRY_USER, user_audio_source_entry_cb, NULL);
}

void app_evt_audio_arbiter_user_action(uint8_t enable)
{
    AUDIO_SOURCE_ENTRY_STATUS ret = app_audio_arbiter_report_source_req(AUDIO_SOURCE_ENTRY_USER, enable ? AUDIO_SOURCE_ENTRY_ACTION_START_REQ: AUDIO_SOURCE_ENTRY_ACTION_STOP_REQ);

    LOGI("ret %d", ret);
}
