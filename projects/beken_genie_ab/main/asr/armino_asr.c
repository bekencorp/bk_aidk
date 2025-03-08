#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#if (CONFIG_SYS_CPU1)
#include "aud_tras_drv.h"
#endif
#include "bk_wanson_asr.h"
#include "armino_asr.h"

#if (CONFIG_SYS_CPU2)
#include "media_mailbox_list_util.h"
#include "media_evt.h"
#endif

#define TAG "armino_asr"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


static wanson_asr_handle_t gl_wanson_asr = NULL;
#if (CONFIG_SYS_CPU2)
static media_mailbox_msg_t asr_to_media_major_msg = {0};
#endif

int aec_output_callback(void *asr_data, void *user_data)
{
    asr_data_t *asr_data_ptr = (asr_data_t *)asr_data;

    LOGD("%s, %p, %d\n", __func__, asr_data_ptr->data, asr_data_ptr->size);

    return bk_wanson_asr_data_write(gl_wanson_asr, (int16_t *)asr_data_ptr->data, asr_data_ptr->size);
}

static int wanson_asr_result_notify_handle(wanson_asr_handle_t wanson_asr, char *result, void *params)
{
    uint32_t asr_result = 0;

    if (os_strcmp(result, "嗨阿米诺") == 0)                 //识别出唤醒词 嗨阿米诺
    {
        LOGI("%s \n", "hi armino, cmd: 0 ");
        asr_result = 1;
    }
    else if (os_strcmp(result, "嘿阿米楼") == 0)
    {
        LOGI("%s \n", "hi armino, cmd: 1 ");
        asr_result = 1;
    }
    else if (os_strcmp(result, "嘿儿米楼") == 0)
    {
        LOGI("%s \n", "hi armino, cmd: 2 ");
        asr_result = 1;
    }
    else if (os_strcmp(result, "嘿鹅迷楼") == 0)
    {
        LOGI("%s \n", "hi armino, cmd: 3 ");
        asr_result = 1;
    }
    else if (os_strcmp(result, "拜拜阿米诺") == 0)     //识别出 拜拜阿米诺
    {
        LOGI("%s \n", "byebye armino, cmd: 0 ");
        asr_result = 2;
    }
    else if (os_strcmp(result, "拜拜阿米楼") == 0)
    {
        LOGI("%s \n", "byebye armino, cmd: 1 ");
        asr_result = 2;
    }
    else
    {
        //nothing
    }

    if (asr_result > 0)
    {
#if (CONFIG_SYS_CPU1)
        aud_tras_drv_set_dialog_run_state_by_asr_result(asr_result);
#endif

#if (CONFIG_SYS_CPU2)
        asr_to_media_major_msg.event = EVENT_ASR_RESULT_NOTIFY;
        asr_to_media_major_msg.result = asr_result;
        msg_send_notify_to_media_minor_mailbox(&asr_to_media_major_msg, MAJOR_MODULE);
#endif
    }

    return BK_OK;
}

bk_err_t armino_asr_open(void)
{
    wanson_asr_cfg_t asr_config = DEFAULT_WANSON_ASR_CONFIG();
    asr_config.asr_result_notify = wanson_asr_result_notify_handle;
    gl_wanson_asr = bk_wanson_asr_create(&asr_config);
    if (!gl_wanson_asr)
    {
        BK_LOGE(TAG, "%s, %d, create wanson asr handle fail\n", __func__, __LINE__);
        return BK_FAIL;
    }
    LOGI("audio wanson asr complete\n");

#if (CONFIG_SYS_CPU1)
    aud_tras_drv_register_aec_ouput_callback(aec_output_callback, NULL);
#endif

    bk_wanson_asr_start(gl_wanson_asr);
    LOGI("wanson asr start complete\n");

    return BK_OK;
}

bk_err_t armino_asr_close(void)
{
    if (gl_wanson_asr)
    {
        bk_wanson_asr_destroy(gl_wanson_asr);
        gl_wanson_asr = NULL;
    }

    return BK_OK;
}


