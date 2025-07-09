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
#include "audio_engine.h"
#include "audio_log.h"
#include "cli.h"

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"

#if CONFIG_SYS_CPU0
bool tx_mic_data_flag = false;
bool rx_spk_data_flag = false;

#ifdef AUDIO_RX_SPK_DATA_DUMP
static uart_util_t g_audio_spk_uart_util = {0};
#endif

#endif //ONFIG_SYS_CPU0

#if CONFIG_SYS_CPU1
extern bool aec_all_data_flag;
extern bool dec_data_flag;
extern void aud_set_production_mode(int val);
#endif  //ONFIG_SYS_CPU1

uint8_t dump_flag = 0;
#endif//CONFIG_DEBUG_DUMP


void cli_beken_aud_dump_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
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
            dump_flag |= (1<<DUMP_TYPE_TX_MIC);
            tx_mic_data_flag = true;
            os_printf("dump beken tx mic data\n!");
        }
        else
        {
            dump_flag &= (~(1<<DUMP_TYPE_TX_MIC));
            tx_mic_data_flag = false;
        }
    }
    else if (os_strcmp(argv[1], "dump_rx_spk_data") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            dump_flag |= (1<<DUMP_TYPE_RX_SPK);
            rx_spk_data_flag = true;
            os_printf("dump beken rx spk data\n!");
        }
        else
        {
            dump_flag &= (~(1<<DUMP_TYPE_RX_SPK));
            rx_spk_data_flag = false;
        }
    }
    else if (os_strcmp(argv[1], "dump_stop") == 0)
    {
        tx_mic_data_flag = false;
        rx_spk_data_flag = false;
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
    else if (os_strcmp(argv[1], "dump_dec_data") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            dump_flag |= (1<<DUMP_TYPE_DEC_OUT_DATA);
            dec_data_flag = true;
            os_printf("dump beken aud dec data\n!");
        }
        else
        {
            dump_flag &= (~(1<<DUMP_TYPE_DEC_OUT_DATA));
            dec_data_flag = false;
        }
    }
    else if (os_strcmp(argv[1], "dump_stop") == 0)
    {
        aec_all_data_flag = false;
        dec_data_flag = false;
        dump_flag = 0;
        os_printf("dump stop\n!");
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
    AUDE_LOGW("beken_debug {dump_mic_data|dump_aec_all_data|dump_rx_mic_data value}\n");
#else
    os_printf("beken_debug not enable:check CONFIG_DEBUG_DUMP!\n!");
#endif//CONFIG_DEBUG_DUMP
}

#if (CONFIG_SYS_CPU1)
void cli_beken_sweep_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{

    if (os_strcmp(argv[1], "enable") == 0)
    {
        aud_set_production_mode(1);
        os_printf("sweep_test_enable\n!");
    }
    else if (os_strcmp(argv[1], "disable") == 0)
    {
        aud_set_production_mode(0);
        os_printf("sweep_test_disable\n!");
    }   
}
#endif

#if (CONFIG_SYS_CPU0)
#define AUDIO_DUMP_CMD_CNT   (sizeof(s_audio_dump_commands) / sizeof(struct cli_command))

static const struct cli_command s_audio_dump_commands[] =
{
    {"audio_dump", "audio_dump ...", cli_beken_aud_dump_cmd},
};

int audio_dump_data_cli_init(void)
{
    return cli_register_commands(s_audio_dump_commands, AUDIO_DUMP_CMD_CNT);
}

#elif CONFIG_SYS_CPU1

#define AUDIO_DUMP_CMD_CNT   (sizeof(s_audio_dump_commands) / sizeof(struct cli_command))
static const struct cli_command s_audio_dump_commands[] =
{
    {"audio_dump", "audio_dump ...", cli_beken_aud_dump_cmd},
    {"sweep_test", "sweep_test ...", cli_beken_sweep_test_cmd},

};

int audio_dump_data_cli_init(void)
{
    return cli_register_commands(s_audio_dump_commands, AUDIO_DUMP_CMD_CNT);
}

#endif
