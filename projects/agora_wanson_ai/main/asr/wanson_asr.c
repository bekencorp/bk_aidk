#include <common/bk_include.h>
#include <modules/pm.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#include "wanson_asr.h"
#include "asr.h"

#include "audio_record.h"

#include "audio_control.h"

#define TAG "ws_asr"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#define RAW_READ_SIZE    (960)

static beken_thread_t wanson_asr_task_hdl = NULL;
static beken_queue_t wanson_asr_msg_que = NULL;
static beken_semaphore_t wanson_asr_sem = NULL;
static audio_record_t *gl_audio_record = NULL;

int8_t *asr_buff = NULL;
const static char *text;
static float score;
static int rs;

static char result0[13] = {0xE5, 0xB0, 0x8F, 0xE8, 0x9C, 0x82, 0xE7, 0xAE, 0xA1, 0xE5, 0xAE, 0xB6, 0x00}; //小蜂管家
static char result1[13] = {0xE9, 0x98, 0xBF, 0xE5, 0xB0, 0x94, 0xE7, 0xB1, 0xB3, 0xE8, 0xAF, 0xBA, 0x00}; //阿尔米诺
static char result2[13] = {0xE4, 0xBC, 0x9A, 0xE5, 0xAE, 0xA2, 0xE6, 0xA8, 0xA1, 0xE5, 0xBC, 0x8F, 0x00}; //会客模式
static char result3[13] = {0xE7, 0x94, 0xA8, 0xE9, 0xA4, 0x90, 0xE6, 0xA8, 0xA1, 0xE5, 0xBC, 0x8F, 0x00}; //用餐模式
static char resulta[13] = {0xE7, 0xA6, 0xBB, 0xE5, 0xBC, 0x80, 0xE6, 0xA8, 0xA1, 0xE5, 0xBC, 0x8F, 0x00}; //离开模式
static char resultc[13] = {0xE5, 0x9B, 0x9E, 0xE5, 0xAE, 0xB6, 0xE6, 0xA8, 0xA1, 0xE5, 0xBC, 0x8F, 0x00}; //回家模式


static bk_err_t wanson_asr_send_msg(wanson_asr_op_t op, void *param)
{
    bk_err_t ret;
    wanson_asr_msg_t msg;

    msg.op = op;
    msg.param = param;
    if (wanson_asr_msg_que)
    {
        ret = rtos_push_to_queue(&wanson_asr_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("send_mic_data_send_msg fail \r\n");
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}

static void wanson_asr_task_main(beken_thread_arg_t param_data)
{
    bk_err_t ret = BK_OK;
    int read_size = 0;

    uint8_t *aud_temp_data = (uint8_t *)psram_malloc(RAW_READ_SIZE);
    if (!aud_temp_data)
    {
        LOGE("malloc aud_temp_data fail\n");
        goto wanson_asr_exit;
    }
    os_memset(aud_temp_data, 0, RAW_READ_SIZE);

    if (Wanson_ASR_Init() < 0)
    {
        LOGE("Wanson_ASR_Init Failed!\n");
        goto wanson_asr_exit;
    }
    Wanson_ASR_Reset();
    LOGI("Wanson_ASR_Init OK!\n");

    wanson_asr_op_t task_state = WANSON_ASR_IDLE;

    rtos_set_semaphore(&wanson_asr_sem);

    wanson_asr_msg_t msg;
    uint32_t wait_time = BEKEN_WAIT_FOREVER;
    while (1)
    {
        ret = rtos_pop_from_queue(&wanson_asr_msg_que, &msg, wait_time);
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
                    LOGI("wanson_asr detecting \"armino\"\n");
                    task_state = WANSON_ASR_START;
                    wait_time = 0;
                    break;

                default:
                    break;
            }
        }

        /* read mic data and send */
        if (task_state == WANSON_ASR_START)
        {
            read_size = audio_record_read_data(gl_audio_record, (char *)aud_temp_data, RAW_READ_SIZE);
            if (read_size == RAW_READ_SIZE)
            {
                rs = Wanson_ASR_Recog((short *)aud_temp_data, 480, &text, &score);
                if (rs == 1)
                {
                    if (os_strcmp(text, result0) == 0)      //识别出唤醒词 小蜂管家
                    {
                        LOGI("%s \n", "xiao feng guan jia ");
                    }
                    else if (os_strcmp(text, result1) == 0)        //识别出唤醒词 阿尔米诺
                    {
                        LOGI("%s \n", "++++++++++++>> ASR Result: armino");
                        wanson_asr_stop();
                        LOGI("wanson_asr stop detecting\n");
                        audio_control_send_msg(AUDIO_CONTROL_BELL, NULL);
                    }
                    else if (os_strcmp(text, result2) == 0)        //识别出 会客模式
                    {
                        LOGI("%s \n", "hui ke mo shi ");
                    }
                    else if (os_strcmp(text, result3) == 0)      //识别出 用餐模式
                    {
                        LOGI("%s \n", "yong can mo shi ");
                    }
                    else if (os_strcmp(text, resulta) == 0)      //识别出 离开模式
                    {
                        LOGI("%s \n", "li kai mo shi ");
                    }
                    else if (os_strcmp(text, resultc) == 0)      //识别出 回家模式
                    {
                        LOGI("%s \n", "hui jia mo shi ");
                    }
                    else
                    {
                        //nothing
                    }
                }
            }
            else
            {
                LOGE("wanson_read_mic_data fail, read_size: %d \n", read_size);
            }
        }

    }

wanson_asr_exit:
    if (aud_temp_data)
    {
        psram_free(aud_temp_data);
        aud_temp_data == NULL;
    }

    Wanson_ASR_Release();

    /* delete msg queue */
    ret = rtos_deinit_queue(&wanson_asr_msg_que);
    if (ret != kNoErr)
    {
        LOGE("delete message queue fail \n");
    }
    wanson_asr_msg_que = NULL;

    /* delete task */
    wanson_asr_task_hdl = NULL;

    LOGI("delete wanson_asr task\n");

    rtos_set_semaphore(&wanson_asr_sem);

    rtos_delete_thread(NULL);
}

static bk_err_t wanson_asr_main_task_init(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&wanson_asr_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_init_queue(&wanson_asr_msg_que,
                          "wanson_asr_que",
                          sizeof(wanson_asr_msg_t),
                          2);
    if (ret != kNoErr)
    {
        LOGE("create wanson asr message queue fail\n");
        goto fail;
    }
    LOGI("create wanson asr message queue complete\n");

    ret = rtos_create_thread(&wanson_asr_task_hdl,
                             6,
                             "wanson_asr",
                             (beken_thread_function_t)wanson_asr_task_main,
                             1024,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create wanson_asr task fail\n", __func__, __LINE__);
        wanson_asr_msg_que = NULL;
        goto fail;
    }

    rtos_get_semaphore(&wanson_asr_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("init wanson_asr task complete \n");

    return BK_OK;

fail:

    if (wanson_asr_sem)
    {
        rtos_deinit_semaphore(&wanson_asr_sem);
        wanson_asr_sem = NULL;
    }

    if (wanson_asr_msg_que)
    {
        rtos_deinit_queue(&wanson_asr_msg_que);
        wanson_asr_msg_que = NULL;
    }


    return BK_FAIL;
}


bk_err_t wanson_asr_init(audio_record_t *record)
{
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);

    gl_audio_record = record;

    /* init send mic data task */
    wanson_asr_main_task_init();

    return BK_OK;
}

bk_err_t wanson_asr_deinit(void)
{
    bk_err_t ret = BK_OK;

    ret = wanson_asr_send_msg(WANSON_ASR_EXIT, NULL);
    if (ret != BK_OK)
    {
        return BK_OK;
    }

    rtos_get_semaphore(&wanson_asr_sem, BEKEN_NEVER_TIMEOUT);
    rtos_deinit_semaphore(&wanson_asr_sem);
    wanson_asr_sem = NULL;

    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_DEFAULT);

    return BK_OK;
}

bk_err_t wanson_asr_start(void)
{
    wanson_asr_send_msg(WANSON_ASR_START, NULL);

    return BK_OK;
}

bk_err_t wanson_asr_stop(void)
{
    wanson_asr_send_msg(WANSON_ASR_IDLE, NULL);

    /* wait read mic data complete and set state to idle */
    //TODO

    return BK_OK;
}

