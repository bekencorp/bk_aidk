// Copyright 2025-2026 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <common/bk_include.h>
#include <modules/pm.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include "bk_wanson_asr.h"
#include "asr.h"
#include "ring_buffer.h"


#define TAG "ws_asr"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#define RAW_READ_SIZE    (960)


#if(CONFIG_WANSON_ASR_GROUP_VERSION)
Fst fst_1;
Fst fst_2;
static unsigned char asr_curr_group_id; // 当前使用的分组ID
#endif
static uint8_t wanson_fst_group_select = 0;

typedef enum {
    WANSON_ASR_IDLE = 0,
    WANSON_ASR_START,
    WANSON_ASR_EXIT
} wanson_asr_op_t;

typedef struct {
    wanson_asr_op_t op;
    void *param;
} wanson_asr_msg_t;

struct wanson_asr
{
    beken_thread_t task_hdl;
    beken_queue_t msg_que;
    beken_semaphore_t sem;

    int8_t *asr_buff;
    char *text;
    float score;
    int rs;

    ringbuf_handle_t pool_rb;                       /**< read pool ringbuffer handle */
    uint32_t pool_size;                             /**< read pool size, unit byte */

    wanson_asr_result_notify asr_result_notify;     /*!< call this notify when wanson asr get result */
    void *usr_data;                                 /*!< the parameter of asr_result_notify callback */
};


static bk_err_t wanson_asr_send_msg(beken_queue_t *msg_que, wanson_asr_op_t op, void *param)
{
    bk_err_t ret;
    wanson_asr_msg_t msg;

    msg.op = op;
    msg.param = param;
    if (*msg_que)
    {
        ret = rtos_push_to_queue(msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("%s, %d, send_msg fail, ret: %d\n", __func__, __LINE__, ret);
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}

#if(CONFIG_WANSON_ASR_GROUP_VERSION)
/**
 * @brief 分组设置 
 * 
 * 当前设备没有播放音乐时，切换成分组1
 * 当设备需要播放音乐前，将其切换成分组2
 * 
 * @param group_id 分组ID
 */
void wanson_fst_group_change(unsigned char group_id)
{
    /* 如果需要设置的分组和当前分组ID不一致，则进行切换 */
    if(asr_curr_group_id != group_id) {
        asr_curr_group_id = group_id;

        if (group_id == 1) {
            Wanson_ASR_Set_Fst(&fst_1);
        } else if (group_id == 2) {
            Wanson_ASR_Set_Fst(&fst_2);
        }
        LOGD("fst_group_change_to: %d\n", group_id);
    }
}
#endif

static void wanson_asr_task_main(beken_thread_arg_t param_data)
{
    bk_err_t ret = BK_OK;
    int read_size = 0;

    wanson_asr_handle_t wanson_asr = (wanson_asr_handle_t)param_data;

#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
    uint8_t *mic_data = (uint8_t *)os_malloc(RAW_READ_SIZE);
#else
    uint8_t *mic_data = (uint8_t *)psram_malloc(RAW_READ_SIZE);
#endif
    if (!mic_data)
    {
        LOGE("%s, %d, malloc mic_data fail\n", __func__, __LINE__);
        goto wanson_asr_exit;
    }
    os_memset(mic_data, 0, RAW_READ_SIZE);

    if (Wanson_ASR_Init() < 0)
    {
        LOGE("%s, %d, Wanson_ASR_Init Fail\n", __func__, __LINE__);
        goto wanson_asr_exit;
    }
#if (CONFIG_WANSON_ASR_GROUP_VERSION)
    /* 指令分组初始化 */
    fst_1.states = fst01_states;
    fst_1.num_states = fst01_num_states;
    fst_1.finals = fst01_finals;
    fst_1.num_finals = fst01_num_finals;
    fst_1.words = fst01_words;

    fst_2.states = fst02_states;
    fst_2.num_states = fst02_num_states;
    fst_2.finals = fst02_finals;
    fst_2.num_finals = fst02_num_finals;
    fst_2.words = fst02_words;

    /* 设置默认分组 */
    wanson_fst_group_change(2); // 默认设置分组一
    bk_printf("Wanson_ASR_Init GRP OK!\n");
#else
    Wanson_ASR_Reset();
#endif
    
    LOGI("Wanson_ASR_Init OK!\n");

    wanson_asr_op_t task_state = WANSON_ASR_IDLE;

    rtos_set_semaphore(&wanson_asr->sem);

    wanson_asr_msg_t msg;
    uint32_t wait_time = BEKEN_WAIT_FOREVER;
    while (1)
    {
        ret = rtos_pop_from_queue(&wanson_asr->msg_que, &msg, wait_time);
        if (kNoErr == ret)
        {
            switch (msg.op)
            {
                case WANSON_ASR_IDLE:
                    LOGI("wanson_asr idle\n");
                    task_state = WANSON_ASR_IDLE;
                    wait_time = BEKEN_WAIT_FOREVER;
                    break;

                case WANSON_ASR_EXIT:
                    LOGD("goto: WANSON_ASR_EXIT \n");
                    goto wanson_asr_exit;
                    break;

                case WANSON_ASR_START:
                    LOGI("wanson_asr detecting \"hi armino\"\n");
                    task_state = WANSON_ASR_START;
                    wait_time = 0;
                    break;

                default:
                    break;
            }
        }

        /* read mic data and wanson asr */
        if (task_state == WANSON_ASR_START)
        {
            read_size = rb_read(wanson_asr->pool_rb, (char *)mic_data, RAW_READ_SIZE, 40);
            if (read_size == RAW_READ_SIZE)
            {
                if(wanson_fst_group_select == 1)
                {
                    wanson_fst_group_change(1);
                }else if(wanson_fst_group_select == 2)
                {
                    wanson_fst_group_change(2);
                }
                wanson_asr->rs = Wanson_ASR_Recog((short*)mic_data, 480, (const char **)&wanson_asr->text, &wanson_asr->score);
                if (wanson_asr->rs == 1)
                {
                    if (wanson_asr->asr_result_notify)
                    {
                        wanson_asr->asr_result_notify(wanson_asr, wanson_asr->text, wanson_asr->usr_data);
                    }
#if 0
                    if (os_strcmp(wanson_asr->text, "嗨阿米诺") == 0)           //识别出唤醒词 嗨阿米诺
                    {
                        LOGI("%s \n", "hi armino ");
                    }
                    else if (os_strcmp(wanson_asr->text, "拜拜阿米诺") == 0)      //识别出 拜拜阿米诺
                    {
                        LOGI("%s \n", "byebye armino ");
                    }
                    else
                    {
                        //nothing
                    }
#endif
                }
            }
            else
            {
                LOGD("%s, %d, wanson_read_mic_data fail, read_size: %d\n", __func__, __LINE__, read_size);
            }
        }
    }

wanson_asr_exit:
    if (mic_data)
    {
#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
        os_free(mic_data);
#else
        psram_free(mic_data);
#endif
        mic_data = NULL;
    }

    Wanson_ASR_Release();

    /* delete msg queue */
    ret = rtos_deinit_queue(&wanson_asr->msg_que);
    if (ret != kNoErr) {
        LOGE("%s, %d, delete message queue fail\n", __func__, __LINE__);
    }
    wanson_asr->msg_que = NULL;

    /* delete task */
    wanson_asr->task_hdl = NULL;

    LOGI("delete wanson_asr task\n");

    rtos_set_semaphore(&wanson_asr->sem);

    rtos_delete_thread(NULL);
}

static bk_err_t wanson_asr_main_task_init(wanson_asr_handle_t wanson_asr)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&wanson_asr->sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_init_queue(&wanson_asr->msg_que,
                          "wanson_asr_que",
                          sizeof(wanson_asr_msg_t),
                          5);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create wanson asr message queue fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_create_thread(&wanson_asr->task_hdl,
                             5,
                             "wanson_asr",
                             (beken_thread_function_t)wanson_asr_task_main,
                             1024,
                             wanson_asr);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create wanson_asr task fail\n", __func__, __LINE__);
        goto fail;
    }
    os_printf("%s, %d\n", __func__, __LINE__);

    rtos_get_semaphore(&wanson_asr->sem, BEKEN_NEVER_TIMEOUT);

    if (!wanson_asr->msg_que)
    {
        LOGE("%s, %d, wanson_asr task run fail\n", __func__, __LINE__);
        goto fail;
    }

    LOGI("init wanson_asr task complete\n");

    return BK_OK;

fail:

    if (wanson_asr->sem)
    {
        rtos_deinit_semaphore(&wanson_asr->sem);
        wanson_asr->sem = NULL;
    }

    if (wanson_asr->msg_que)
    {
        rtos_deinit_queue(&wanson_asr->msg_que);
        wanson_asr->msg_que = NULL;
    }

    return BK_FAIL;
}

wanson_asr_handle_t bk_wanson_asr_create(wanson_asr_cfg_t *config)
{
#if (CONFIG_SYS_CPU0 || CONFIG_SYS_CPU1)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);
#endif
    bk_err_t ret = BK_OK;

#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
    wanson_asr_handle_t wanson_asr = (wanson_asr_handle_t)os_malloc(sizeof(struct wanson_asr));
#else
    wanson_asr_handle_t wanson_asr = (wanson_asr_handle_t)psram_malloc(sizeof(struct wanson_asr));
#endif
    if (!wanson_asr)
    {
        LOGE("%s, %d, os_malloc wanson_asr_handle: %d fail\n", __func__, __LINE__, sizeof(struct wanson_asr));
        return NULL;
    }
    os_memset(wanson_asr, 0, sizeof(struct wanson_asr));

    wanson_asr->asr_result_notify = config->asr_result_notify;
    wanson_asr->usr_data = config->usr_data;

    /* init pool ringbuffer */
    wanson_asr->pool_size = config->pool_size;
    wanson_asr->pool_rb = rb_create(wanson_asr->pool_size);
    if (!wanson_asr->pool_rb)
    {
        LOGE("%s, %d, create pool ringbuffer: %d fail\n", __func__, __LINE__, wanson_asr->pool_size);
        goto fail;
    }

    /* init send mic data task */
    ret = wanson_asr_main_task_init(wanson_asr);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, wanson asr task init fail, ret: %d\n", __func__, __LINE__, ret);
        goto fail;
    }

    return wanson_asr;

fail:

    if (wanson_asr && wanson_asr->pool_rb)
    {
        rb_destroy(wanson_asr->pool_rb);
        wanson_asr->pool_rb = NULL;
    }

    if (wanson_asr)
    {
#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
        os_free(wanson_asr);
#else
        psram_free(wanson_asr);
#endif
        wanson_asr = NULL;
    }

    return NULL;
}

bk_err_t bk_wanson_asr_destroy(wanson_asr_handle_t wanson_asr)
{
    bk_err_t ret = BK_OK;

    if (!wanson_asr)
    {
        LOGE("%s, %d, wanson_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = wanson_asr_send_msg(&wanson_asr->msg_que, WANSON_ASR_EXIT, NULL);
    if (ret != BK_OK)
    {
        return BK_OK;
    }

    rtos_get_semaphore(&wanson_asr->sem, BEKEN_NEVER_TIMEOUT);
    rtos_deinit_semaphore(&wanson_asr->sem);
    wanson_asr->sem = NULL;

#if (CONFIG_SYS_CPU0 || CONFIG_SYS_CPU1)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_DEFAULT);
#endif

    if (wanson_asr->pool_rb)
    {
        rb_destroy(wanson_asr->pool_rb);
        wanson_asr->pool_rb = NULL;
    }

#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
    os_free(wanson_asr);
#else
    psram_free(wanson_asr);
#endif
    wanson_asr = NULL;

    return BK_OK;
}

bk_err_t bk_wanson_asr_start(wanson_asr_handle_t wanson_asr)
{
    if (!wanson_asr)
    {
        LOGE("%s, %d, wanson_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    wanson_asr_send_msg(&wanson_asr->msg_que, WANSON_ASR_START, NULL);

    return BK_OK;
}

bk_err_t bk_wanson_asr_stop(wanson_asr_handle_t wanson_asr)
{
    if (!wanson_asr)
    {
        LOGE("%s, %d, wanson_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    wanson_asr_send_msg(&wanson_asr->msg_que, WANSON_ASR_IDLE, NULL);

    /* wait read mic data complete and set state to idle */
    //TODO

    return BK_OK;
}

int bk_wanson_asr_data_write(wanson_asr_handle_t wanson_asr, int16_t *buffer, uint32_t len)
{
    if (!wanson_asr)
    {
        LOGE("%s, %d, wanson_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    return rb_write(wanson_asr->pool_rb, (char *)buffer, len, 0);
}

void bk_wanson_asr_set_spk_play_flag(uint8 spk_play_flag)
{
    if(spk_play_flag)
    {
        wanson_fst_group_select =  1;
    }
    else
    {
        wanson_fst_group_select = 2;
    }
}


