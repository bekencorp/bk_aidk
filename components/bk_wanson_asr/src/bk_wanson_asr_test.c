// Copyright 2023-2024 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdio.h"
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <modules/pm.h>
#include "audio_record.h"
#include "bk_wanson_asr.h"
#include "cli.h"


#define TAG  "ws_asr_test"

static bool wanson_asr_running = false;
static beken_thread_t wanson_asr_task_hdl = NULL;
static beken_semaphore_t wanson_asr_sem = NULL;

static wanson_asr_handle_t gl_wanson_asr = NULL;
static audio_record_t *gl_aud_record = NULL;


static bk_err_t wanson_asr_test_exit(void)
{
    wanson_asr_running = false;

    if (gl_wanson_asr)
    {
        bk_wanson_asr_destroy(gl_wanson_asr);
    }

    if (gl_aud_record)
    {
        audio_record_destroy(gl_aud_record);
    }

    return BK_OK;
}

static int wanson_asr_result_notify_handle(wanson_asr_handle_t wanson_asr, char *result, void *params)
{
    if (os_strcmp(result, "小蜂管家") == 0)          //识别出唤醒词 小蜂管家
    {
        BK_LOGI("%s \n", "xiao feng guan jia ");
    }
    else if (os_strcmp(result, "阿尔米诺") == 0)     //识别出唤醒词 阿尔米诺
    {
        BK_LOGI("%s \n", "a er mi nuo ");
    }
    else if (os_strcmp(result, "会客模式") == 0)     //识别出 会客模式
    {
        BK_LOGI("%s \n", "hui ke mo shi ");
    }
    else if (os_strcmp(result, "用餐模式") == 0)     //识别出 用餐模式
    {
        BK_LOGI("%s \n", "yong can mo shi ");
    }
    else if (os_strcmp(result, "离开模式") == 0)     //识别出 离开模式
    {
        BK_LOGI("%s \n", "li kai mo shi ");
    }
    else if (os_strcmp(result, "回家模式") == 0)     //识别出 回家模式
    {
        BK_LOGI("%s \n", "hui jia mo shi ");
    }
    else
    {
        //nothing
    }

    return BK_OK;
}


static void wanson_asr_main(beken_thread_arg_t param_data)
{
    char *mic_data_buff = NULL;

    /* init audio record */
    audio_record_cfg_t record_config = DEFAULT_AUDIO_RECORD_CONFIG();
    record_config.sampRate = 16000;
    record_config.frame_size = 960;
    record_config.pool_size = 1920;
    gl_aud_record = audio_record_create(AUDIO_RECORD_ONBOARD_MIC, &record_config);
    if (!gl_aud_record)
    {
        BK_LOGE(TAG, "%s, %d, create audio record handle fail\n", __func__, __LINE__);
        goto fail;
    }
    BK_LOGI(TAG, "audio record create complete\n");

    wanson_asr_cfg_t asr_config = DEFAULT_WANSON_ASR_CONFIG();
    asr_config.asr_result_notify = wanson_asr_result_notify_handle;
    gl_wanson_asr = bk_wanson_asr_create(&asr_config);
    if (!gl_wanson_asr)
    {
        BK_LOGE(TAG, "%s, %d, create wanson asr handle fail\n", __func__, __LINE__);
        goto fail;
    }
    BK_LOGI(TAG, "audio wanson asr complete\n");

#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
    mic_data_buff = os_malloc(960);
#else
    mic_data_buff = psram_malloc(960);
#endif
    if (!mic_data_buff)
    {
        BK_LOGE(TAG, "%s, %d, malloc mic_data_buff: 960 fail\n", __func__, __LINE__);
        goto fail;
    }
    os_memset(mic_data_buff, 0, 960);

    bk_wanson_asr_start(gl_wanson_asr);
    BK_LOGI(TAG, "wanson asr start complete\n");

    audio_record_open(gl_aud_record);
    BK_LOGI(TAG, "audio record open complete\n");

    rtos_set_semaphore(&wanson_asr_sem);

    wanson_asr_running = true;

    while (wanson_asr_running)
    {
        bk_err_t read_size = audio_record_read_data(gl_aud_record, mic_data_buff, 960);
        if (read_size == 960)
        {
            int write_size = bk_wanson_asr_data_write(gl_wanson_asr, (int16_t *)mic_data_buff, 960);
            if (write_size != 960)
            {
                BK_LOGE(TAG, "%s, %d, wanson_asr write size: %d != 960\n", __func__, __LINE__, write_size);
            }
        }
        else
        {
            BK_LOGE(TAG, "%s, %d, audio_record read size: %d != 960\n", __func__, __LINE__, read_size);
        }
    }

fail:
    wanson_asr_running = false;

    wanson_asr_test_exit();

    if (mic_data_buff)
    {
#ifdef CONFIG_BEKEN_WANSON_ASR_USE_SRAM
        os_free(mic_data_buff);
#else
        psram_free(mic_data_buff);
#endif
        mic_data_buff = NULL;
    }

    rtos_set_semaphore(&wanson_asr_sem);

    wanson_asr_task_hdl = NULL;

    rtos_delete_thread(NULL);
}

static bk_err_t wanson_asr_test_start(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&wanson_asr_sem, 1);
    if (ret != BK_OK)
    {
        BK_LOGE(TAG, "%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_create_thread(&wanson_asr_task_hdl,
                             (BEKEN_DEFAULT_WORKER_PRIORITY - 1),
                             "audio_play",
                             (beken_thread_function_t)wanson_asr_main,
                             2048,
                             NULL);
    if (ret != BK_OK)
    {
        BK_LOGE(TAG, "%s, %d, create wanson_asr_main task fail\n", __func__, __LINE__);
        goto fail;
    }

    rtos_get_semaphore(&wanson_asr_sem, BEKEN_NEVER_TIMEOUT);

    BK_LOGI(TAG, "%s, %d, create wanson_asr_main task complete\n", __func__, __LINE__);

    return BK_OK;

fail:
    if (wanson_asr_sem)
    {
        rtos_deinit_semaphore(&wanson_asr_sem);
        wanson_asr_sem = NULL;
    }

    return BK_FAIL;
}


static bk_err_t wanson_asr_test_stop(void)
{
    if (!wanson_asr_running)
    {
        BK_LOGW(TAG, "%s, %d, wanson_asr not work\n", __func__, __LINE__);
        return BK_OK;
    }

    wanson_asr_running = false;

    rtos_get_semaphore(&wanson_asr_sem, BEKEN_NEVER_TIMEOUT);

    rtos_deinit_semaphore(&wanson_asr_sem);
    wanson_asr_sem = NULL;

    return BK_OK;
}


static void cli_wanson_asr_test_help(void)
{
    BK_LOGI(TAG, "wanson_asr_test {start|stop}\n");
}

void cli_wanson_asr_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc != 2)
    {
        cli_wanson_asr_test_help();
        return;
    }

    if (os_strcmp(argv[1], "start") == 0)
    {
        wanson_asr_test_start();
    }
    else if (os_strcmp(argv[1], "stop") == 0)
    {
        wanson_asr_test_stop();
    }
    else
    {
        cli_wanson_asr_test_help();
    }
}


#define WANSON_ASR_TEST_CMD_CNT  (sizeof(s_wanson_asr_test_commands) / sizeof(struct cli_command))
static const struct cli_command s_wanson_asr_test_commands[] =
{
	{"wanson_asr_test", "wanson_asr_test {start|stop}", cli_wanson_asr_test_cmd},
};

int cli_wanson_asr_test_init(void)
{
	BK_LOGI(TAG, "cli_wanson_asr_test_init \n");

	return cli_register_commands(s_wanson_asr_test_commands, WANSON_ASR_TEST_CMD_CNT);
}


