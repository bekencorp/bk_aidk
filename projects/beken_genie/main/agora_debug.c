#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <components/shell_task.h>
#include <components/event.h>
#include <components/netif_types.h>
#include "bk_rtos_debug.h"

#define TAG "agora_debug"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

bool agoora_tx_mic_data_flag = false;
#if CONFIG_SYS_CPU1
extern bool aec_all_data_flag;
#endif

void cli_agora_rtc_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc < 3)
    {
        goto cmd_fail;
    }

    /* audio test */
    if (os_strcmp(argv[1], "dump_mic_data") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            agoora_tx_mic_data_flag = true;
        }
        else
        {
            agoora_tx_mic_data_flag = false;
        }
    }
    #if CONFIG_SYS_CPU1
    /* audio test */
    if (os_strcmp(argv[1], "dump_aec_all_data") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            aec_all_data_flag = true;
        }
        else
        {
            aec_all_data_flag = false;
        }
    }
    #endif
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    LOGI("agora_debug {dump_mic_data|dump_aec_all_data value}\n");
}


