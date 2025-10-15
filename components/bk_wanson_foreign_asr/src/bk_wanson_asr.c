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
#include <driver/otp.h>

#include "bk_wanson_asr.h"
#include "asr.h"
#include "ring_buffer.h"
#include "foreign_model.h"

#include "DbgTrace.h"

#define TAG "ws_asr"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#define RAW_READ_SIZE    (960)



typedef enum {
    WANSON_ASR_IDLE = 0,
    WANSON_ASR_START,
    WANSON_ASR_EXIT,
    WANSON_ASR_MODEL_CHANGE,
    WANSON_ASR_GROUP_CHANGE
} wanson_asr_op_t;

typedef struct {
    wanson_asr_op_t op;
    const void* param;
} wanson_asr_msg_t;

struct wanson_asr {
    beken_thread_t task_hdl;
    beken_queue_t msg_que;
    beken_semaphore_t sem;

    int8_t* asr_buff;
    char* text;
    float score;
    int rs;

    ringbuf_handle_t pool_rb;                       /**< read pool ringbuffer handle */
    uint32_t pool_size;                             /**< read pool size, unit byte */

    wanson_asr_result_notify asr_result_notify;     /*!< call this notify when wanson asr get result */
    void* usr_data;                                 /*!< the parameter of asr_result_notify callback */
};


wanson_asr_handle_t wanson_asr = NULL;
static unsigned char cur_group_index = 0;

const char* model_names[] = {
    "english_air.bin",
    "japanese_air.bin",
};

static bk_err_t wanson_asr_send_msg(beken_queue_t* msg_que, wanson_asr_op_t op, const void* param)
{
    bk_err_t ret;
    wanson_asr_msg_t msg;

    msg.op = op;
    msg.param = param;
    if (*msg_que) {
        ret = rtos_push_to_queue(msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret) {
            LOGE("%s, %d, send_msg fail, ret: %d\n", __func__, __LINE__, ret);
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}

void wanson_kws_algorithm_deinit()
{
    Wanson_ASR_Release();
}


int wanson_kws_model_init(const char* model_name)
{
    unsigned int* start_addr = NULL;
    unsigned int* end_addr = NULL;
    get_bin_addresses(model_name, &start_addr, &end_addr);
    if (start_addr != NULL && end_addr != NULL) {
        size_t size = (const unsigned char*)end_addr - (const unsigned char*)start_addr;
        DBG_TRACE("Size of %s: start_addr->%02X,end_addr->%02X ; %zu bytes\n",
            model_name, (const unsigned char*)end_addr, (const unsigned char*)start_addr, size);
    } else {
        DBG_TRACE("Model %s not found.\n", model_name);
        return -1;
    }

    if (Wanson_Model_Init(start_addr, end_addr) < 0) {
        DBG_TRACE("Wanson_Model_Init fail\n");
        return -1;
    }
    return 0;
}

int wanson_kws_algorithm_init()
{
    unsigned int* start_addr = NULL;
    unsigned int* end_addr = NULL;
    for (int i = 0; i < sizeof(model_names) / sizeof(model_names[0]); i++) {
        get_bin_addresses(model_names[i], &start_addr, &end_addr);
        if (start_addr != NULL && end_addr != NULL) {
            size_t size = (const unsigned char*)end_addr - (const unsigned char*)start_addr;
            DBG_TRACE("Size of %s: start_addr->%02X,end_addr->%02X ; %zu bytes\n",
                model_names[i], (const unsigned char*)end_addr, (const unsigned char*)start_addr, size);
        } else {
            DBG_TRACE("Model %s not found.\n", model_names[i]);
            return -1;
        }
    }

    if (Wanson_ASR_Init() < 0) {
        DBG_TRACE("Wanson_ASR_Init fail\n");
        return -1;
    }

    if (Wanson_ASR_Reset(cur_group_index) < 0) {
        DBG_TRACE("Wanson_ASR_Reset fail\n");
        return -1;
    }

    DBG_TRACE("Wanson_ASR_Init OK!\n");
    return 0;
}

void wanson_fst_group_change(unsigned char value)
{
    if (wanson_asr) {
        if (cur_group_index != value) {
            cur_group_index = value; 
            wanson_asr_send_msg(&wanson_asr->msg_que, WANSON_ASR_GROUP_CHANGE, &cur_group_index);
        }
    }
}

static void wanson_asr_task_main(beken_thread_arg_t param_data)
{
    bk_err_t ret = BK_OK;
    int read_size = 0;

    // wanson_asr_handle_t wanson_asr = (wanson_asr_handle_t)param_data;
    wanson_asr = (wanson_asr_handle_t)param_data;

#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
    uint8_t* mic_data = (uint8_t*)os_malloc(RAW_READ_SIZE);
#else
    uint8_t* mic_data = (uint8_t*)psram_malloc(RAW_READ_SIZE);
#endif
    if (!mic_data) {
        LOGE("%s, %d, malloc mic_data fail\n", __func__, __LINE__);
        goto wanson_asr_exit;
    }
    os_memset(mic_data, 0, RAW_READ_SIZE);

    if (wanson_kws_model_init(model_names[0]) < 0) {
        DBG_TRACE("wanson_kws_model_init fail\n");
        goto wanson_asr_exit;
    }
    DBG_TRACE("wanson_kws_model_init OK!\n");

    if (wanson_kws_algorithm_init() < 0) {
        DBG_TRACE("wanson_kws_algorithm_init fail\n");
        goto wanson_asr_exit;
    }
    DBG_TRACE("wanson_kws_algorithm_init OK!\n");

    wanson_asr_op_t task_state = WANSON_ASR_IDLE;

    rtos_set_semaphore(&wanson_asr->sem);
 
    wanson_asr_msg_t msg;
    uint32_t wait_time = BEKEN_WAIT_FOREVER;
    while (1) {
        ret = rtos_pop_from_queue(&wanson_asr->msg_que, &msg, wait_time);
        if (kNoErr == ret) {
            switch (msg.op) {
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

            case WANSON_ASR_MODEL_CHANGE:
                task_state = WANSON_ASR_MODEL_CHANGE;
                DBG_TRACE("goto: WANSON_ASR_MODEL_CHANGE %s\r\n", msg.param);
                wanson_kws_algorithm_deinit();
                DBG_TRACE("wanson_kws_algorithm_deinit OK!\n");

                if (wanson_kws_model_init(msg.param) < 0) {
                    DBG_TRACE("wanson_kws_model_init fail\n");
                    goto wanson_asr_exit;
                }
                DBG_TRACE("wanson_kws_model_init OK!\n");

                if (wanson_kws_algorithm_init() < 0) {
                    DBG_TRACE("wanson_kws_algorithm_init fail\n");
                    goto wanson_asr_exit;
                }
                DBG_TRACE("wanson_kws_algorithm_init OK!\n");
                // wanson_asr_send_msg(&wanson_asr->msg_que, WANSON_ASR_START, NULL);
                task_state = WANSON_ASR_START;
                wait_time = 0;
                break;

            case WANSON_ASR_GROUP_CHANGE:
                task_state = WANSON_ASR_GROUP_CHANGE;
                unsigned char value = *(unsigned char*)msg.param;
                if (Wanson_ASR_Get_Activ_Group_Index() != value) {
                    // DBG_TRACE("wanson_fst_group_change111 %d\n", value);
                    if (Wanson_ASR_Reset(value) < 0) {
                        DBG_TRACE("WANSON_ASR_GROUP_CHANGE fail\n");
                        goto wanson_asr_exit;
                    }

                }
                DBG_TRACE("WANSON_ASR_GROUP_CHANGE SUCC !!! %d\r\n", value);
                task_state = WANSON_ASR_START;
                wait_time = 0;
                break;
            default:
                break;
            }
        }

        /* read mic data and wanson asr */
        if (task_state == WANSON_ASR_START) {
            read_size = rb_read(wanson_asr->pool_rb, (char*)mic_data, RAW_READ_SIZE, 40);
            if (read_size == RAW_READ_SIZE) {
                // unsigned  long  start = rtos_get_time();
                wanson_asr->rs = Wanson_ASR_Recog((short*)mic_data, 480, (const char**)&wanson_asr->text, &wanson_asr->score);
                // unsigned  long  end = rtos_get_time();
                // bk_printf("%ld", end - start);
                if (wanson_asr->rs == 1) {
                    os_printf(" ASR Result: %s\n", wanson_asr->text);	 //识别结果打印
                    // wanson_asr_send_msg(&wanson_asr->msg_que, WANSON_ASR_MODEL_CHANGE, model_names[1]);
                    if (wanson_asr->asr_result_notify) {
                        wanson_asr->asr_result_notify(wanson_asr, wanson_asr->text, wanson_asr->usr_data);
                    }
                }
            } else {
                LOGD("%s, %d, wanson_read_mic_data fail, read_size: %d\n", __func__, __LINE__, read_size);
            }
        }
    }

wanson_asr_exit:
    // Release wanson
    wanson_kws_algorithm_deinit();

    if (mic_data) {
#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
        os_free(mic_data);
#else
        psram_free(mic_data);
#endif
        mic_data = NULL;
    }



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
    if (ret != BK_OK) {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_init_queue(&wanson_asr->msg_que,
        "wanson_asr_que",
        sizeof(wanson_asr_msg_t),
        5);
    if (ret != kNoErr) {
        LOGE("%s, %d, create wanson asr message queue fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_create_thread(&wanson_asr->task_hdl,
        5,
        "wanson_asr",
        (beken_thread_function_t)wanson_asr_task_main,
        1024 * 4,
        wanson_asr);
    if (ret != kNoErr) {
        LOGE("%s, %d, create wanson_asr task fail\n", __func__, __LINE__);
        goto fail;
    }
    os_printf("%s, %d\n", __func__, __LINE__);

    rtos_get_semaphore(&wanson_asr->sem, BEKEN_NEVER_TIMEOUT);

    if (!wanson_asr->msg_que) {
        LOGE("%s, %d, wanson_asr task run fail\n", __func__, __LINE__);
        goto fail;
    }

    LOGI("init wanson_asr task complete\n");

    return BK_OK;

fail:

    if (wanson_asr->sem) {
        rtos_deinit_semaphore(&wanson_asr->sem);
        wanson_asr->sem = NULL;
    }

    if (wanson_asr->msg_que) {
        rtos_deinit_queue(&wanson_asr->msg_que);
        wanson_asr->msg_que = NULL;
    }

    return BK_FAIL;
}

wanson_asr_handle_t bk_wanson_asr_create(wanson_asr_cfg_t* config)
{
#if (CONFIG_SYS_CPU0 || CONFIG_SYS_CPU1)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);
#endif
    bk_err_t ret = BK_OK;

    //   bk_printf(">>>>>>>>>>>%d\r\n",rtos_get_free_heap_size())  ;
#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
    wanson_asr_handle_t wanson_asr = (wanson_asr_handle_t)os_malloc(sizeof(struct wanson_asr));
#else
    wanson_asr_handle_t wanson_asr = (wanson_asr_handle_t)psram_malloc(sizeof(struct wanson_asr));
#endif
    if (!wanson_asr) {
        LOGE("%s, %d, os_malloc wanson_asr_handle: %d fail\n", __func__, __LINE__, sizeof(struct wanson_asr));
        return NULL;
    }
    os_memset(wanson_asr, 0, sizeof(struct wanson_asr));

    wanson_asr->asr_result_notify = config->asr_result_notify;
    wanson_asr->usr_data = config->usr_data;

    /* init pool ringbuffer */
    wanson_asr->pool_size = config->pool_size;
    wanson_asr->pool_rb = rb_create(wanson_asr->pool_size);
    if (!wanson_asr->pool_rb) {
        LOGE("%s, %d, create pool ringbuffer: %d fail\n", __func__, __LINE__, wanson_asr->pool_size);
        goto fail;
    }

    /* init send mic data task */
    ret = wanson_asr_main_task_init(wanson_asr);
    if (ret != BK_OK) {
        LOGE("%s, %d, wanson asr task init fail, ret: %d\n", __func__, __LINE__, ret);
        goto fail;
    }

    return wanson_asr;

fail:

    if (wanson_asr && wanson_asr->pool_rb) {
        rb_destroy(wanson_asr->pool_rb);
        wanson_asr->pool_rb = NULL;
    }

    if (wanson_asr) {
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

    if (!wanson_asr) {
        LOGE("%s, %d, wanson_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = wanson_asr_send_msg(&wanson_asr->msg_que, WANSON_ASR_EXIT, NULL);
    if (ret != BK_OK) {
        return BK_OK;
    }

    rtos_get_semaphore(&wanson_asr->sem, BEKEN_NEVER_TIMEOUT);
    rtos_deinit_semaphore(&wanson_asr->sem);
    wanson_asr->sem = NULL;

#if (CONFIG_SYS_CPU0 || CONFIG_SYS_CPU1)
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_DEFAULT);
#endif

    if (wanson_asr->pool_rb) {
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
    if (!wanson_asr) {
        LOGE("%s, %d, wanson_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    wanson_asr_send_msg(&wanson_asr->msg_que, WANSON_ASR_START, NULL);

    return BK_OK;
}

bk_err_t bk_wanson_asr_stop(wanson_asr_handle_t wanson_asr)
{
    if (!wanson_asr) {
        LOGE("%s, %d, wanson_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    wanson_asr_send_msg(&wanson_asr->msg_que, WANSON_ASR_IDLE, NULL);

    /* wait read mic data complete and set state to idle */
    //TODO

    return BK_OK;
}

int bk_wanson_asr_data_write(wanson_asr_handle_t wanson_asr, int16_t* buffer, uint32_t len)
{
    if (!wanson_asr) {
        LOGE("%s, %d, wanson_asr is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    return rb_write(wanson_asr->pool_rb, (char*)buffer, len, 0);
}

void bk_wanson_asr_set_spk_play_flag(uint8 spk_play_flag)
{
    if (spk_play_flag) {
        //非打断换场景：喇叭没播放
        wanson_fst_group_change(0);
    } else {
        //唤醒打断场景：喇叭正在播放 
        wanson_fst_group_change(1);
    }
}


