#include <common/bk_include.h>
#include <modules/pm.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#include "wenwen_asr.h"
#include "mobvoi_bk7258_pipeline.h"
#include "audio_record.h"
#include "audio_control.h"

#define TAG "ww_asr"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#define RAW_READ_SIZE    (320)

static beken_thread_t wenwen_asr_task_hdl = NULL;
static beken_queue_t wenwen_asr_msg_que = NULL;
static beken_semaphore_t wenwen_asr_sem = NULL;
static audio_record_t *gl_audio_record = NULL;
static void *wenwen_asr_inst = NULL;

static bk_err_t wenwen_asr_send_msg(wenwen_asr_op_t op, void *param)
{
    bk_err_t ret;
    wenwen_asr_msg_t msg;

    msg.op = op;
    msg.param = param;
    if (wenwen_asr_msg_que)
    {
        ret = rtos_push_to_queue(&wenwen_asr_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("wenwen_asr_send_msg fail \r\n");
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}

static void wenwen_asr_task_main(beken_thread_arg_t param_data)
{
    bk_err_t ret = BK_OK;
    int read_size = 0;
    void *mob_memory = NULL;
    static bool asr_active = false;

    uint8_t *aud_temp_data = (uint8_t *)psram_malloc(RAW_READ_SIZE);
    if (!aud_temp_data)
    {
        LOGE("malloc aud_temp_data fail\n");
        goto wenwen_asr_exit;
    }
    os_memset(aud_temp_data, 0, RAW_READ_SIZE);

    /* wenwen asr init */
    unsigned int wenwen_asr_mem_size = mobvoi_dsp_get_memory_needed();
    LOGI("wenwen_asr_mem_size: %d\n", wenwen_asr_mem_size);
    wenwen_asr_mem_size = (wenwen_asr_mem_size + 4) & (~4);
    mob_memory = os_malloc(wenwen_asr_mem_size);
    if (mob_memory == NULL)
    {
        LOGE("malloc asr_memory: %d fail\n", wenwen_asr_mem_size);
        goto wenwen_asr_exit;
    }
    os_memset(mob_memory, 0, wenwen_asr_mem_size);

    mobvoi_dsp_set_memory_base(mob_memory, wenwen_asr_mem_size);

    wenwen_asr_inst = mobvoi_bk7258_pipeline_init(16000, 10);
    if (wenwen_asr_inst == NULL)
    {
        LOGE("wenwen asr init fail\n");
        goto wenwen_asr_exit;
    }
    LOGI("Wenwen asr init OK!\n");

    wenwen_asr_op_t task_state = WENWEN_ASR_IDLE;

    rtos_set_semaphore(&wenwen_asr_sem);

    wenwen_asr_msg_t msg;
    uint32_t wait_time = BEKEN_WAIT_FOREVER;
    while (1)
    {
        ret = rtos_pop_from_queue(&wenwen_asr_msg_que, &msg, wait_time);
        if (kNoErr == ret)
        {
            switch (msg.op)
            {
                case WENWEN_ASR_IDLE:
                    LOGI("wenwen_asr idle\n");
                    task_state = WENWEN_ASR_IDLE;
                    wait_time = BEKEN_WAIT_FOREVER;
                    break;

                case WENWEN_ASR_EXIT:
                    LOGD("goto: WENWEN_ASR_EXIT \n");
                    goto wenwen_asr_exit;
                    break;

                case WENWEN_ASR_START:
                    LOGI("wenwen_asr detecting \"hi, armino\"\n");
                    task_state = WENWEN_ASR_START;
                    wait_time = 0;
                    break;

                default:
                    break;
            }
        }

        /* read mic data and wenwen asr process */
        if (task_state == WENWEN_ASR_START)
        {
            read_size = audio_record_read_data(gl_audio_record, (char *)aud_temp_data, RAW_READ_SIZE);
            if (read_size == RAW_READ_SIZE)
            {
                int16_t out_data[160] = {0};
                int result = mobvoi_bk7258_pipeline_process(wenwen_asr_inst, (short *)aud_temp_data, (short *)aud_temp_data, out_data);
                if (result == 100 && asr_active == false)
                {
                    asr_active = true;
                    LOGI("%s \n", "++++++++++++>> ASR Result: hi, armino");
                    wenwen_asr_stop();
                    LOGI("wenwen_asr stop detecting\n");
                    audio_control_send_msg(AUDIO_CONTROL_BELL, NULL);
                }
                else if (result == -1)
                {
                    asr_active = false;
                }
                else
                {
                    LOGD("result: %d\n", result);
                }
            }
            else
            {
                LOGE("wenwen_read_mic_data fail, read_size: %d \n", read_size);
            }
        }
    }

wenwen_asr_exit:
    if (aud_temp_data)
    {
        psram_free(aud_temp_data);
        aud_temp_data == NULL;
    }

    if (mob_memory)
    {
        psram_free(mob_memory);
        mob_memory == NULL;
    }

    if (wenwen_asr_inst)
    {
        mobvoi_bk7258_pipeline_cleanup(wenwen_asr_inst);
        wenwen_asr_inst = NULL;
    }

    /* delete msg queue */
    ret = rtos_deinit_queue(&wenwen_asr_msg_que);
    if (ret != kNoErr)
    {
        LOGE("delete message queue fail \n");
    }
    wenwen_asr_msg_que = NULL;

    /* delete task */
    wenwen_asr_task_hdl = NULL;

    LOGI("delete wenwen_asr task\n");

    rtos_set_semaphore(&wenwen_asr_sem);

    rtos_delete_thread(NULL);
}

static bk_err_t wenwen_asr_main_task_init(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&wenwen_asr_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_init_queue(&wenwen_asr_msg_que,
                          "wenwen_asr_que",
                          sizeof(wenwen_asr_msg_t),
                          2);
    if (ret != kNoErr)
    {
        LOGE("create wenwen asr message queue fail\n");
        goto fail;
    }
    LOGI("create wenwen asr message queue complete\n");

    ret = rtos_create_thread(&wenwen_asr_task_hdl,
                             6,
                             "wenwen_asr",
                             (beken_thread_function_t)wenwen_asr_task_main,
                             8192,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create wenwen_asr task fail\n", __func__, __LINE__);
        wenwen_asr_msg_que = NULL;
        goto fail;
    }

    rtos_get_semaphore(&wenwen_asr_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("init wenwen_asr task complete \n");

    return BK_OK;

fail:

    if (wenwen_asr_sem)
    {
        rtos_deinit_semaphore(&wenwen_asr_sem);
        wenwen_asr_sem = NULL;
    }

    if (wenwen_asr_msg_que)
    {
        rtos_deinit_queue(&wenwen_asr_msg_que);
        wenwen_asr_msg_que = NULL;
    }


    return BK_FAIL;
}


bk_err_t wenwen_asr_init(audio_record_t *record)
{
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);

    gl_audio_record = record;

    /* init wenwen asr task */
    wenwen_asr_main_task_init();

    return BK_OK;
}

bk_err_t wenwen_asr_deinit(void)
{
    bk_err_t ret = BK_OK;

    ret = wenwen_asr_send_msg(WENWEN_ASR_EXIT, NULL);
    if (ret != BK_OK)
    {
        return BK_OK;
    }

    rtos_get_semaphore(&wenwen_asr_sem, BEKEN_NEVER_TIMEOUT);
    rtos_deinit_semaphore(&wenwen_asr_sem);
    wenwen_asr_sem = NULL;

    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_DEFAULT);

    return BK_OK;
}

bk_err_t wenwen_asr_start(void)
{
    wenwen_asr_send_msg(WENWEN_ASR_START, NULL);

    return BK_OK;
}

bk_err_t wenwen_asr_stop(void)
{
    wenwen_asr_send_msg(WENWEN_ASR_IDLE, NULL);

    /* wait read mic data complete and set state to idle */
    //TODO

    return BK_OK;
}

