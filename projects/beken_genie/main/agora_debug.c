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
#include "aud_intf.h"
#include "aud_intf_types.h"
#include <modules/audio_process.h>

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"

#if CONFIG_SYS_CPU0
bool agoora_tx_mic_data_flag = false;
bool agoora_rx_spk_data_flag = false;
#elif CONFIG_SYS_CPU1
extern bool aec_all_data_flag;
#if CONFIG_AUD_INTF_SUPPORT_OPUS
bool agoora_tx_mic_data_flag = false;
#endif
extern void aud_set_production_mode(int val);
#else
#endif

uint8_t dump_flag = 0;
#endif//CONFIG_DEBUG_DUMP


#define TAG "agora_d"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

void cli_agora_rtc_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
#if CONFIG_DEBUG_DUMP
    uint8_t dump_flag_pre = dump_flag;
    if (argc < 2)
    {
        goto cmd_fail;
    }

    /* audio test */
    #if CONFIG_SYS_CPU0
    if (os_strcmp(argv[1], "dump_mic_data") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            dump_flag |= (1<<DUMP_TYPE_AGORA_TX_MIC);
            agoora_tx_mic_data_flag = true;
            os_printf("dump agora tx mic data\n!");
        }
        else
        {
            dump_flag &= (~(1<<DUMP_TYPE_AGORA_TX_MIC));
            agoora_tx_mic_data_flag = false;
        }
    }
    else if (os_strcmp(argv[1], "dump_rx_spk_data") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            dump_flag |= (1<<DUMP_TYPE_AGORA_RX_SPK);
            agoora_rx_spk_data_flag = true;
            os_printf("dump agora rx spk data\n!");
        }
        else
        {
            dump_flag &= (~(1<<DUMP_TYPE_AGORA_RX_SPK));
            agoora_rx_spk_data_flag = false;
        }
    }
    else if (os_strcmp(argv[1], "dump_stop") == 0)
    {
        agoora_tx_mic_data_flag = false;
        agoora_rx_spk_data_flag = false;
        dump_flag = 0;
        os_printf("dump stop\n!");
    }
    else if (os_strcmp(argv[1], "set_vad_para") == 0)
    {
        int16_t vad_start_thr = (os_strtoul(argv[2], NULL, 10));
        int16_t vad_stop_thr = (os_strtoul(argv[3], NULL, 10));
        int16_t vad_silence_thr = (os_strtoul(argv[4], NULL, 10));
        int16_t vad_eng_thr = (os_strtoul(argv[5], NULL, 10));

        bk_aud_intf_set_vad_para(AUD_INTF_VOC_VAD_START_THRESHOLD,vad_start_thr);
        bk_aud_intf_set_vad_para(AUD_INTF_VOC_VAD_STOP_THRESHOLD,vad_stop_thr);
        bk_aud_intf_set_vad_para(AUD_INTF_VOC_VAD_SILENCE_THRESHOLD,vad_silence_thr);
        bk_aud_intf_set_vad_para(AUD_INTF_VOC_VAD_ENG_THRESHOLD,vad_silence_thr);
                

        os_printf("set_vad_para %d %d %d %d \r\n",vad_start_thr,vad_stop_thr,vad_silence_thr,vad_eng_thr);
    }
    
    else
    {
        goto cmd_fail;
    }
    #elif CONFIG_SYS_CPU1
    /* audio test */
    if (os_strcmp(argv[1], "dump_aec_all_data") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            dump_flag |= (1<<DUMP_TYPE_AEC_MIC_DATA);
            aec_all_data_flag = true;
            os_printf("dump ace_data\n!");
        }
        else
        {
            dump_flag &= (~(1<<DUMP_TYPE_AEC_MIC_DATA));
            aec_all_data_flag = false;
        }
    }
    else if (os_strcmp(argv[1], "dump_stop") == 0)
    {
        aec_all_data_flag = false;
        dump_flag = 0;
        os_printf("dump stop\n!");
    }
    else if (os_strcmp(argv[1], "sweep_test_enable") == 0)
    {
        aud_set_production_mode(1);
        os_printf("sweep_test_enable\n!");
    }
    else if (os_strcmp(argv[1], "sweep_test_disable") == 0)
    {
        aud_set_production_mode(0);
        os_printf("sweep_test_disable\n!");
    }
    else
    {
        goto cmd_fail;
    }
    #else
    os_printf("only support debug dump function on CPU0 or CPU1 \n!");
    #endif
    

    os_printf("pre dump_flag:0x%x cur:0x%x \n!",dump_flag_pre,dump_flag);

    if((!dump_flag_pre) && (dump_flag))
    {
        os_printf("open dump uart\n!");
        DEBUG_DATA_DUMP_BY_UART_OPEN();
    }

    if((dump_flag_pre) && (!dump_flag))
    {
        os_printf("close dump uart\n!");
        DEBUG_DATA_DUMP_BY_UART_CLOSE();
    }

    return;

cmd_fail:
    LOGI("agora_debug {dump_mic_data|dump_aec_all_data|dump_rx_mic_data value}\n");
#else
    os_printf("agora_debug not enable:check CONFIG_DEBUG_DUMP!\n!");
#endif//CONFIG_DEBUG_DUMP
}


