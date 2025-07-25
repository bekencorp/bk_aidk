#include "app_audio_arbiter.h"
#include "os/os.h"
#include "os/mem.h"

#include <stdint.h>
#include <stdio.h>

#define TAG "app_audio_arb"

enum
{
    AUDIO_ARB_DEBUG_LEVEL_ERROR,
    AUDIO_ARB_DEBUG_LEVEL_WARNING,
    AUDIO_ARB_DEBUG_LEVEL_INFO,
    AUDIO_ARB_DEBUG_LEVEL_DEBUG,
    AUDIO_ARB_DEBUG_LEVEL_VERBOSE,
};

#define AUDIO_ARB_DEBUG_LEVEL AUDIO_ARB_DEBUG_LEVEL_INFO

#define LOGE(format, ...) do{if(AUDIO_ARB_DEBUG_LEVEL >= AUDIO_ARB_DEBUG_LEVEL_ERROR)   BK_LOGE(TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define LOGW(format, ...) do{if(AUDIO_ARB_DEBUG_LEVEL >= AUDIO_ARB_DEBUG_LEVEL_WARNING) BK_LOGW(TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define LOGI(format, ...) do{if(AUDIO_ARB_DEBUG_LEVEL >= AUDIO_ARB_DEBUG_LEVEL_INFO)    BK_LOGI(TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define LOGD(format, ...) do{if(AUDIO_ARB_DEBUG_LEVEL >= AUDIO_ARB_DEBUG_LEVEL_DEBUG)   BK_LOGD(TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define LOGV(format, ...) do{if(AUDIO_ARB_DEBUG_LEVEL >= AUDIO_ARB_DEBUG_LEVEL_VERBOSE) BK_LOGV(TAG, "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)

typedef struct
{
    AUDIO_SOURCE_ENTRY_STATUS status;
    audio_source_entry_cb cb;
    void *arg;
} audio_arbiter_ctx_t;

static const uint8_t s_audio_priority[AUDIO_SOURCE_ENTRY_END] =
{
    [AUDIO_SOURCE_ENTRY_AI] = 9,
    [AUDIO_SOURCE_ENTRY_A2DP] = 8,
    [AUDIO_SOURCE_ENTRY_USER] = 10,
};

static audio_arbiter_ctx_t s_audio_arbiter_ctx[AUDIO_SOURCE_ENTRY_END];
static beken_mutex_t s_mutex;

AUDIO_SOURCE_ENTRY_EMUM app_audio_arbiter_get_current_play_entry(void)
{
    AUDIO_SOURCE_ENTRY_EMUM ret = AUDIO_SOURCE_ENTRY_END;

    if (!s_mutex)
    {
        LOGE("not init !!!!");
        return ret;
    }

    rtos_lock_mutex(&s_mutex);

    for (uint32_t i = 0; i < AUDIO_SOURCE_ENTRY_END; ++i)
    {
        if (s_audio_arbiter_ctx[i].status == AUDIO_SOURCE_ENTRY_STATUS_PLAY)
        {
            ret = i;
            break;
        }
    }

    rtos_unlock_mutex(&s_mutex);

    return ret;
}

AUDIO_SOURCE_ENTRY_STATUS app_audio_arbiter_report_source_req(AUDIO_SOURCE_ENTRY_EMUM entry, AUDIO_SOURCE_ENTRY_ACTION act)
{
    uint8_t current_priority = s_audio_priority[entry];
    uint32_t i = 0;
    AUDIO_SOURCE_ENTRY_STATUS ret = 0;
    int32_t play_entry = -1;
#define LOG_BUFFER_LEN 256

    if (!s_mutex)
    {
        LOGE("not init !!!!");
        return AUDIO_SOURCE_ENTRY_STATUS_STOP;
    }

    char *log_buffer1 = NULL;
    char *log_buffer2 = NULL;
    char *log_buffer3 = NULL;

    log_buffer1 = psram_malloc(LOG_BUFFER_LEN);
    log_buffer2 = psram_malloc(LOG_BUFFER_LEN);
    log_buffer3 = psram_malloc(LOG_BUFFER_LEN);

    const char *log_array[] = {log_buffer1, log_buffer2, log_buffer3};

    if (!log_buffer1 || !log_buffer2 || !log_buffer3)
    {
        LOGE("malloc err");
        goto malloc_err;
    }

    os_memset(log_buffer1, 0, LOG_BUFFER_LEN);
    os_memset(log_buffer2, 0, LOG_BUFFER_LEN);
    os_memset(log_buffer3, 0, LOG_BUFFER_LEN);

    LOGI("try entry %d:%d act %s", entry, current_priority, act == AUDIO_SOURCE_ENTRY_ACTION_START_REQ ? "start" : "stop");

    rtos_lock_mutex(&s_mutex);

    if (act == AUDIO_SOURCE_ENTRY_ACTION_START_REQ &&
            (s_audio_arbiter_ctx[entry].status == AUDIO_SOURCE_ENTRY_STATUS_PLAY
             //|| s_audio_arbiter_ctx[entry].status == AUDIO_SOURCE_ENTRY_STATUS_PLAY_PENDING)
            ))

    {
        ret = s_audio_arbiter_ctx[entry].status;
        LOGI("act %d already match status %d", act, s_audio_arbiter_ctx[entry].status);
        goto end;
    }

    if (act == AUDIO_SOURCE_ENTRY_ACTION_STOP_REQ && s_audio_arbiter_ctx[entry].status == AUDIO_SOURCE_ENTRY_STATUS_STOP)
    {
        ret = s_audio_arbiter_ctx[entry].status;
        LOGI("act %d already match status %d", act, s_audio_arbiter_ctx[entry].status);
        goto end;
    }

    for (i = 0; i < AUDIO_SOURCE_ENTRY_END; ++i)
    {
        if (s_audio_arbiter_ctx[i].status == AUDIO_SOURCE_ENTRY_STATUS_PLAY)
        {
            play_entry = i;
            break;
        }
    }

    //sprintf(log_buffer1,
    LOGI("entry %d:%d status %d act %d, current global %s %d:%d",
         entry, current_priority, s_audio_arbiter_ctx[entry].status, act,
         (play_entry >= 0 ? "is_play" : "not_play"), play_entry, play_entry >= 0 ? s_audio_priority[play_entry] : 0);

    if (act == AUDIO_SOURCE_ENTRY_ACTION_START_REQ)
    {
        if (play_entry >= 0)
        {
            if (s_audio_priority[play_entry] < current_priority && s_audio_arbiter_ctx[play_entry].status == AUDIO_SOURCE_ENTRY_STATUS_PLAY)
            {
                //sprintf(log_buffer2,
                LOGI("entry %d:%d higher prio than %d:%d status %d, instead it !!!", entry, current_priority,
                     play_entry, s_audio_priority[play_entry], s_audio_arbiter_ctx[play_entry].status);

                s_audio_arbiter_ctx[play_entry].status = AUDIO_SOURCE_ENTRY_STATUS_PLAY_PENDING;

                if (s_audio_arbiter_ctx[play_entry].cb)
                {
                    rtos_unlock_mutex(&s_mutex);
                    s_audio_arbiter_ctx[play_entry].cb(AUDIO_SOURCE_ENTRY_CB_EVT_NEED_STOP, s_audio_arbiter_ctx[play_entry].arg);
                    rtos_lock_mutex(&s_mutex);
                }

                ret = s_audio_arbiter_ctx[entry].status = AUDIO_SOURCE_ENTRY_STATUS_PLAY;
            }
            else
            {
                //                sprintf(log_buffer2,
                LOGI("entry %d:%d lower prio than %d:%d status %d, pending current entry !!!", entry, current_priority,
                     play_entry, s_audio_priority[play_entry], s_audio_arbiter_ctx[play_entry].status);

                ret = s_audio_arbiter_ctx[entry].status = AUDIO_SOURCE_ENTRY_STATUS_PLAY_PENDING;
            }
        }
        else
        {
            //sprintf(log_buffer2,
            LOGI("entry %d:%d status %d -> %d play", entry, current_priority, s_audio_arbiter_ctx[entry].status, AUDIO_SOURCE_ENTRY_STATUS_PLAY);
            ret = s_audio_arbiter_ctx[entry].status = AUDIO_SOURCE_ENTRY_STATUS_PLAY;
        }
    }
    else if (act == AUDIO_SOURCE_ENTRY_ACTION_STOP_REQ)
    {
        uint32_t max_priority_entry = 0;
        uint32_t j = AUDIO_SOURCE_ENTRY_END;

        if (entry == play_entry)
        {
            play_entry = -1;
        }

        //        sprintf(log_buffer2,
        LOGI("entry %d:%d status %d -> %d stop", entry, current_priority, s_audio_arbiter_ctx[entry].status, AUDIO_SOURCE_ENTRY_STATUS_STOP);
        ret = s_audio_arbiter_ctx[entry].status = AUDIO_SOURCE_ENTRY_STATUS_STOP;

        if (play_entry < 0)
        {
            for (i = 0; i < AUDIO_SOURCE_ENTRY_END; ++i)
            {
                if (i != entry && s_audio_arbiter_ctx[i].cb && s_audio_arbiter_ctx[i].status == AUDIO_SOURCE_ENTRY_STATUS_PLAY_PENDING
                        && max_priority_entry < s_audio_priority[i])
                {
                    max_priority_entry = s_audio_priority[i];
                    j = i;
                }
                else
                {
                    LOGV("%d:%d status %d, not match, %d", i, s_audio_priority[i], s_audio_arbiter_ctx[i].status, max_priority_entry);
                }
            }

            if (j != AUDIO_SOURCE_ENTRY_END)
            {
                //s_audio_arbiter_ctx[j].status = AUDIO_SOURCE_ENTRY_STATUS_PLAY;

                //                sprintf(log_buffer3,
                LOGI("entry %d:%d status %d need to be %d play", j, s_audio_priority[j], s_audio_arbiter_ctx[j].status, AUDIO_SOURCE_ENTRY_STATUS_PLAY);

                if (s_audio_arbiter_ctx[j].cb)
                {
                    rtos_unlock_mutex(&s_mutex);
                    s_audio_arbiter_ctx[j].cb(AUDIO_SOURCE_ENTRY_CB_EVT_NEED_START, s_audio_arbiter_ctx[j].arg);
                    rtos_lock_mutex(&s_mutex);
                }
            }
            else
            {
                //                sprintf(log_buffer3,
                LOGI("no need to call play any entry");
            }
        }
    }

end:;
    rtos_unlock_mutex(&s_mutex);

    for (int i = 0; i < sizeof(log_array) / sizeof(log_array[0]); ++i)
    {
        if (log_array[i] && log_array[i][0])
        {
            LOGI("%s", log_array[i]);
        }
    }

malloc_err:;

    for (int i = 0; i < sizeof(log_array) / sizeof(log_array[0]); ++i)
    {
        if (log_array[i])
        {
            psram_free((void *)log_array[i]);
            log_array[i] = NULL;
        }
    }

    return ret;
}

void app_audio_arbiter_reg_callback(AUDIO_SOURCE_ENTRY_EMUM entry, audio_source_entry_cb cb, void *arg)
{
    if(!s_mutex)
    {
        LOGE("not init");
        return;
    }

    if (entry >= AUDIO_SOURCE_ENTRY_END)
    {
        LOGE("entry not valid %d", entry);
        return;
    }

    LOGI("entry %d cb %p", entry, cb);
    s_audio_arbiter_ctx[entry].cb = cb;
    s_audio_arbiter_ctx[entry].arg = arg;
}


void app_audio_arbiter_init(void)
{
    int32_t ret = 0;

    if (s_mutex)
    {
        LOGE("mutex already init !!!");
        return;
    }

    ret = rtos_init_mutex(&s_mutex);

    if (ret)
    {
        LOGE("rtos_init_mutex err %d", ret);
        return;
    }

    os_memset(s_audio_arbiter_ctx, 0, sizeof(s_audio_arbiter_ctx));
}

void app_audio_arbiter_deinit(void)
{
    int32_t ret = 0;

    if (s_mutex)
    {
        ret = rtos_deinit_mutex(&s_mutex);

        if (ret)
        {
            LOGE("rtos_deinit_mutex err %d", ret);
            return;
        }

        s_mutex = NULL;
    }

    os_memset(s_audio_arbiter_ctx, 0, sizeof(s_audio_arbiter_ctx));
}
