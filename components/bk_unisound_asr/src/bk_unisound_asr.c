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
#include "ring_buffer.h"
#include "bk_unisound_asr.h"
#include "unisound_if.h"

#include "bk_unisound_authcode.h"

#define TAG "unisound_asr"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define RAW_READ_SIZE    (512)

typedef enum {
    UNISOUND_ASR_IDLE = 0,
    UNISOUND_ASR_START,
    UNISOUND_ASR_EXIT
} unisound_asr_op_t;

typedef struct {
    unisound_asr_op_t op;
    void *param;
} unisound_asr_msg_t;

struct unisound_asr
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

    unisound_asr_result_notify asr_result_notify;   /*!< call this notify when unisound asr get result */
    void *usr_data;                                 /*!< the parameter of asr_result_notify callback */
};

static uint8_t *authcode = NULL;

static bk_err_t unisound_asr_send_msg(beken_queue_t *msg_que, unisound_asr_op_t op, void *param)
{
    bk_err_t ret;
    unisound_asr_msg_t msg;

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

static void unisound_asr_task_main(beken_thread_arg_t param_data)
{
    bk_err_t ret = BK_FAIL;
    int read_size = 0;
    // void *kws_buf = NULL;

    unisound_asr_handle_t unisound_asr = (unisound_asr_handle_t)param_data;

    uint8_t *mic_data = (uint8_t *)os_malloc(RAW_READ_SIZE);
    if (!mic_data)
    {
        LOGE("%s, %d, malloc mic_data fail\n", __func__, __LINE__);
        goto unisound_asr_exit;
    }
    os_memset(mic_data, 0x00, RAW_READ_SIZE);
#if CONFIG_UNISOUND_LICENSE
    if (check_unisound_auth_code() == BK_FAIL) {
        os_printf("auth code error, need check more.\n");
        ret = BK_FAIL;
        goto unisound_asr_exit;
    }
    authcode = get_unisound_auth_code();
    if (!authcode) {
        os_printf("get auth code failed\n");
        ret = BK_FAIL;
        goto unisound_asr_exit;
    } else{
        os_printf("get auth code success. authcode: %s\n", authcode);
    }
#endif
    ret = unisound_kws_init((void *)authcode, 20 * 1024);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, unisound_kws_init fail, ret: %d\n", __func__, __LINE__, ret);
        goto unisound_asr_exit;
    }
    LOGI("Unisound_ASR_Init OK!\n");

    unisound_asr_op_t task_state = UNISOUND_ASR_IDLE;

    rtos_set_semaphore(&unisound_asr->sem);

    unisound_asr_msg_t msg;
    uint32_t wait_time = BEKEN_WAIT_FOREVER;
    while (1)
    {
        ret = rtos_pop_from_queue(&unisound_asr->msg_que, &msg, wait_time);
        if (kNoErr == ret)
        {
            switch (msg.op)
            {
                case UNISOUND_ASR_IDLE:
                    LOGI("unisound_asr idle\n");
                    task_state = UNISOUND_ASR_IDLE;
                    wait_time = BEKEN_WAIT_FOREVER;
                    break;

                case UNISOUND_ASR_EXIT:
                    LOGD("goto: UNISOUND_ASR_EXIT \n");
                    goto unisound_asr_exit;
                    break;

                case UNISOUND_ASR_START:
                    LOGI("unisound_asr detecting\n");
                    task_state = UNISOUND_ASR_START;
                    wait_time = 0;
                    break;

                default:
                    break;
            }
        }

        /* read mic data and unisound asr */
        if (task_state == UNISOUND_ASR_START)
        {
            read_size = rb_read(unisound_asr->pool_rb, (char *)mic_data, RAW_READ_SIZE, 40);
            if (read_size == RAW_READ_SIZE)
            {
                unisound_kws_recognize((signed char*)mic_data, read_size);
			//	extern uint8_t g_unisound_wakeup_detected;
			///	unisound_asr->rs = g_unisound_wakeup_detected;
                //if (unisound_asr->rs == 1)
                //os_printf("[+]%s, g_unisound_wakeup_detected:%d, %p\n", __func__, g_unisound_wakeup_detected, unisound_asr->asr_result_notify);
                {
                    if (unisound_asr->asr_result_notify)
                    {
                        unisound_asr->asr_result_notify(unisound_asr, unisound_asr->text, unisound_asr->usr_data);
                    }
                }
            }
            else
            {
                LOGD("%s, %d, unisound_read_mic_data fail, read_size: %d\n", __func__, __LINE__, read_size);
            }
        }
    }

unisound_asr_exit:
    if (mic_data)
    {
        os_free(mic_data);
        mic_data = NULL;
    }

    if (authcode) {
        os_free(authcode);
        authcode = NULL;
    }

    /* delete msg queue */
    ret = rtos_deinit_queue(&unisound_asr->msg_que);
    if (ret != kNoErr) {
        LOGE("%s, %d, delete message queue fail\n", __func__, __LINE__);
    }
    unisound_asr->msg_que = NULL;

    /* delete task */
    unisound_asr->task_hdl = NULL;

    LOGI("delete unisound_asr task\n");
    rtos_set_semaphore(&unisound_asr->sem);
    rtos_delete_thread(NULL);
}

static bk_err_t unisound_asr_main_task_init(unisound_asr_handle_t unisound_asr)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&unisound_asr->sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_init_queue(&unisound_asr->msg_que,
                          "unisound_asr_que",
                          sizeof(unisound_asr_msg_t),
                          5);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create unisound asr message queue fail\n", __func__, __LINE__);
        goto fail;
    }

    #if 0
    ret = rtos_create_thread(&unisound_asr->task_hdl,
                             5,
                             "unisound_asr",
                             (beken_thread_function_t)unisound_asr_task_main,
                             2048*8,
                             unisound_asr);
    #else
    ret = rtos_create_psram_thread(&unisound_asr->task_hdl,
                             5,
                             "unisound_asr",
                             (beken_thread_function_t)unisound_asr_task_main,
                             2048*8,
                             unisound_asr);
    #endif
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create unisound_asr task fail\n", __func__, __LINE__);
        goto fail;
    }
    os_printf("%s, %d\n", __func__, __LINE__);

    rtos_get_semaphore(&unisound_asr->sem, BEKEN_NEVER_TIMEOUT);

    if (!unisound_asr->msg_que)
    {
        LOGE("%s, %d, unisound_asr task run fail\n", __func__, __LINE__);
        goto fail;
    }

    LOGI("init unisound_asr task complete\n");

    return BK_OK;

fail:

    if (unisound_asr->sem)
    {
        rtos_deinit_semaphore(&unisound_asr->sem);
        unisound_asr->sem = NULL;
    }

    if (unisound_asr->msg_que)
    {
        rtos_deinit_queue(&unisound_asr->msg_que);
        unisound_asr->msg_que = NULL;
    }

    return BK_FAIL;
}

unisound_asr_handle_t bk_unisound_asr_create(unisound_asr_cfg_t *config)
{
#if (CONFIG_SYS_CPU0 || CONFIG_SYS_CPU1)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);
#endif
    bk_err_t ret = BK_OK;

    unisound_asr_handle_t unisound_asr = (unisound_asr_handle_t)os_malloc(sizeof(struct unisound_asr));
    if (!unisound_asr)
    {
        LOGE("%s, %d, os_malloc unisound_asr_handle: %d fail\n", __func__, __LINE__, sizeof(struct unisound_asr));
        return NULL;
    }
    os_memset(unisound_asr, 0, sizeof(struct unisound_asr));

    unisound_asr->asr_result_notify = config->asr_result_notify;
    unisound_asr->usr_data = config->usr_data;

    /* init pool ringbuffer */
    unisound_asr->pool_size = config->pool_size;
    unisound_asr->pool_rb = rb_create(unisound_asr->pool_size);
    if (!unisound_asr->pool_rb)
    {
        LOGE("%s, %d, create pool ringbuffer: %d fail\n", __func__, __LINE__, unisound_asr->pool_size);
        goto fail;
    }

    /* init send mic data task */
    ret = unisound_asr_main_task_init(unisound_asr);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, unisound asr task init fail, ret: %d\n", __func__, __LINE__, ret);
        goto fail;
    }

    return unisound_asr;

fail:

    if (unisound_asr && unisound_asr->pool_rb)
    {
        rb_destroy(unisound_asr->pool_rb);
        unisound_asr->pool_rb = NULL;
    }

    if (unisound_asr)
    {
        os_free(unisound_asr);
        unisound_asr = NULL;
    }

    return NULL;
}

bk_err_t bk_unisound_asr_destroy(unisound_asr_handle_t unisound_asr)
{
    bk_err_t ret = BK_OK;

    if (!unisound_asr)
    {
        LOGE("%s, %d, unisound_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = unisound_asr_send_msg(&unisound_asr->msg_que, UNISOUND_ASR_EXIT, NULL);
    if (ret != BK_OK)
    {
        return BK_OK;
    }

    rtos_get_semaphore(&unisound_asr->sem, BEKEN_NEVER_TIMEOUT);
    rtos_deinit_semaphore(&unisound_asr->sem);
    unisound_asr->sem = NULL;

#if (CONFIG_SYS_CPU0 || CONFIG_SYS_CPU1)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_DEFAULT);
#endif

    if (unisound_asr->pool_rb)
    {
        rb_destroy(unisound_asr->pool_rb);
        unisound_asr->pool_rb = NULL;
    }

    os_free(unisound_asr);
    unisound_asr = NULL;

    return BK_OK;
}

bk_err_t bk_unisound_asr_start(unisound_asr_handle_t unisound_asr)
{
    if (!unisound_asr)
    {
        LOGE("%s, %d, unisound_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    unisound_asr_send_msg(&unisound_asr->msg_que, UNISOUND_ASR_START, NULL);

    return BK_OK;
}

bk_err_t bk_unisound_asr_stop(unisound_asr_handle_t unisound_asr)
{
    if (!unisound_asr)
    {
        LOGE("%s, %d, unisound_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    unisound_asr_send_msg(&unisound_asr->msg_que, UNISOUND_ASR_IDLE, NULL);

    return BK_OK;
}

int bk_unisound_asr_data_write(unisound_asr_handle_t unisound_asr, int16_t *buffer, uint32_t len)
{
    if (!unisound_asr)
    {
        LOGE("%s, %d, unisound_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    return rb_write(unisound_asr->pool_rb, (char *)buffer, len, 0);
}