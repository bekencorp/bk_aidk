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

#if CONFIG_ASR_ENGINE_WANSON || CONFIG_ASR_ENGINE_WANSON_FOREIGN
#include "bk_wanson_asr.h"
#elif CONFIG_ASR_ENGINE_UNISOUND
#include "bk_unisound_asr.h"
#endif

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

#if CONFIG_ASR_ENGINE_WANSON || CONFIG_ASR_ENGINE_WANSON_FOREIGN
static wanson_asr_handle_t gl_asr_handle = NULL;
#elif CONFIG_ASR_ENGINE_UNISOUND
static unisound_asr_handle_t gl_asr_handle = NULL;
#endif

#if (CONFIG_SYS_CPU2)
static media_mailbox_msg_t asr_to_media_major_msg = {0};
#endif

int aec_output_callback(void *asr_data, void *user_data)
{
    asr_data_t *asr_data_ptr = (asr_data_t *)asr_data;

    LOGD("%s, %p, %d %d \n", __func__, asr_data_ptr->data, asr_data_ptr->size, asr_data_ptr->spk_play_flag);

#if CONFIG_ASR_ENGINE_WANSON || CONFIG_ASR_ENGINE_WANSON_FOREIGN
    bk_wanson_asr_set_spk_play_flag(asr_data_ptr->spk_play_flag);
    return bk_wanson_asr_data_write(gl_asr_handle, (int16_t *)asr_data_ptr->data, asr_data_ptr->size);
#elif CONFIG_ASR_ENGINE_UNISOUND
    return bk_unisound_asr_data_write(gl_asr_handle, (int16_t *)asr_data_ptr->data, asr_data_ptr->size);
#endif
}


#if (CONFIG_ASR_ENGINE_WANSON_FOREIGN)
static int wanson_asr_result_notify_handle(wanson_asr_handle_t wanson_asr, char *result, void *params)
{
    uint32_t asr_result = 0;

    if (os_strcmp(result, "Hey Alice") == 0)                 //识别出唤醒词 HeyAlice
    {
        LOGI("%s \n", "nihao armino, cmd: 0 ");
        asr_result = 1;
    }
    else if (os_strcmp(result, "Bye Bye Alice") == 0)
    {
        LOGI("%s \n", "zaijian armino, cmd: 1 ");
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

#else
#if CONFIG_ASR_ENGINE_WANSON
static int wanson_asr_result_notify_handle(wanson_asr_handle_t wanson_asr, char *result, void *params)
#elif CONFIG_ASR_ENGINE_UNISOUND
static int unisound_asr_result_notify_handle(unisound_asr_handle_t unisound_asr, char *result, void *params)
#endif
{
    uint32_t asr_result = 0;
#if CONFIG_ASR_ENGINE_WANSON
#if (CONFIG_WANSON_ASR_GROUP_VERSION_WORDS_V1)
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
#else

    if (os_strcmp(result, "你好阿米诺") == 0)                 //识别出唤醒词 你好阿米诺
    {
        LOGI("%s \n", "nihao armino, cmd: 0 ");
        asr_result = 1;
    }
    else if (os_strcmp(result, "再见阿米诺") == 0)
    {
        LOGI("%s \n", "zaijian armino, cmd: 1 ");
        asr_result = 2;
    }
    else
    {
        //nothing
    }

#endif
#endif

#if CONFIG_ASR_ENGINE_UNISOUND
	extern uint8_t g_unisound_wakeup_detected;
	asr_result = g_unisound_wakeup_detected;
	g_unisound_wakeup_detected = 0;
#endif


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
#endif

bk_err_t armino_asr_open(void)
{
#if CONFIG_ASR_ENGINE_WANSON || CONFIG_ASR_ENGINE_WANSON_FOREIGN
    wanson_asr_cfg_t asr_config = DEFAULT_WANSON_ASR_CONFIG();
    asr_config.asr_result_notify = wanson_asr_result_notify_handle;
    gl_asr_handle = bk_wanson_asr_create(&asr_config);
    if (!gl_asr_handle)
    {
        BK_LOGE(TAG, "%s, %d, create wanson asr handle fail\n", __func__, __LINE__);
        return BK_FAIL;
    }
    LOGI("audio wanson asr complete\n");
#elif CONFIG_ASR_ENGINE_UNISOUND
    unisound_asr_cfg_t asr_config = DEFAULT_UNISOUND_ASR_CONFIG();
    asr_config.asr_result_notify = unisound_asr_result_notify_handle;
    gl_asr_handle = bk_unisound_asr_create(&asr_config);
    if (!gl_asr_handle)
    {
        BK_LOGE(TAG, "%s, %d, create unisound asr handle fail\n", __func__, __LINE__);
        return BK_FAIL;
    }
    LOGI("audio unisound asr complete\n");
#endif

#if (CONFIG_SYS_CPU1)
    aud_tras_drv_register_aec_ouput_callback(aec_output_callback, NULL);
#endif

#if CONFIG_ASR_ENGINE_WANSON || CONFIG_ASR_ENGINE_WANSON_FOREIGN
    bk_wanson_asr_start(gl_asr_handle);
    LOGI("wanson asr start complete\n");
#elif CONFIG_ASR_ENGINE_UNISOUND
    bk_unisound_asr_start(gl_asr_handle);
    LOGI("unisound asr start complete\n");
#endif

    return BK_OK;
}

bk_err_t armino_asr_close(void)
{
    if (gl_asr_handle)
    {
#if CONFIG_ASR_ENGINE_WANSON || CONFIG_ASR_ENGINE_WANSON_FOREIGN
        bk_wanson_asr_destroy(gl_asr_handle);
#elif CONFIG_ASR_ENGINE_UNISOUND
        bk_unisound_asr_destroy(gl_asr_handle);
#endif
        gl_asr_handle = NULL;
    }

    return BK_OK;
}


