#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <driver/audio_ring_buff.h>
#include "audio_transfer.h"
#include "aud_intf.h"
#include <modules/audio_process.h>
#include "audio_engine.h"
#include "audio_log.h"

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
#endif

//#define TX_MIC_DATA_DUMP

#ifdef TX_MIC_DATA_DUMP
#include "uart_util.h"
static uart_util_t g_mic_uart_util = {0};
#define TX_MIC_DATA_DUMP_UART_ID            (1)
#define TX_MIC_DATA_DUMP_UART_BAUD_RATE     (2000000)

#define TX_MIC_DATA_DUMP_OPEN()                        uart_util_create(&g_mic_uart_util, TX_MIC_DATA_DUMP_UART_ID, TX_MIC_DATA_DUMP_UART_BAUD_RATE)
#define TX_MIC_DATA_DUMP_CLOSE()                       uart_util_destroy(&g_mic_uart_util)
#define TX_MIC_DATA_DUMP_DATA(data_buf, len)           uart_util_tx_data(&g_mic_uart_util, data_buf, len)
#else
#define TX_MIC_DATA_DUMP_OPEN()
#define TX_MIC_DATA_DUMP_CLOSE()
#define TX_MIC_DATA_DUMP_DATA(data_buf, len)
#endif  //TX_MIC_DATA_DUMP


typedef enum
{
    AUD_TRAS_TX_DATA = 0,

    AUD_TRAS_EXIT,
} aud_tras_op_t;

typedef struct
{
    aud_tras_op_t op;
} aud_tras_msg_t;

static beken_thread_t  aud_thread_hdl = NULL;
static beken_queue_t aud_msg_que = NULL;
static beken_semaphore_t aud_sem = NULL;
static RingBufferContext mic_data_rb;
static uint8_t *mic_data_buffer = NULL;
#if CONFIG_AUD_VAD_SUPPORT
static uint8_t *mic_drop_data = NULL;
#endif
static user_audio_tx_data_func audio_tx_func = NULL;

#define MIC_FRAME_NUM 4
#define PRE_VAD_START_FRAME_NUM 2 
#if CONFIG_AUD_INTF_SUPPORT_OPUS
static RingBufferContext mic_data_len_rb;
static uint8_t *mic_data_len_buffer = NULL;
#endif
static uint16_t mic_tx_buf_frame_num = MIC_FRAME_NUM;

extern bool tx_mic_data_flag;
extern bool g_connected_flag;

enum vad_state
{
    VAD_NONE              = (0x00),
    VAD_SPEECH_START      = (0x01),
    VAD_SPEECH_END        = (0x02),
};

static int send_audio_frame(uint8_t *data, unsigned int len)
{
    // audio_frame_info_t info = { 0 };
    if(!g_connected_flag)
    {
        return 0;
    }

    #if CONFIG_DEBUG_DUMP
    if (tx_mic_data_flag)
    {
        uint32_t encoder_type = bk_aud_get_encoder_type();
        uint8_t dump_file_type = dbg_dump_get_dump_file_type((uint8_t)encoder_type);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DUMP_FILE_TYPE(DUMP_TYPE_TX_MIC,0,dump_file_type);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_TX_MIC,0,len);
        DEBUG_DATA_DUMP_UPDATE_HEADER_TIMESTAMP(DUMP_TYPE_TX_MIC);
        #if CONFIG_DEBUG_DUMP_DATA_TYPE_EXTENSION
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_SAMP_RATE(DUMP_TYPE_TX_MIC,0,bk_aud_get_adc_sample_rate());
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_FRAME_IN_MS(DUMP_TYPE_TX_MIC,0,bk_aud_get_enc_frame_len_in_ms());
        #endif
        DEBUG_DATA_DUMP_BY_UART_HEADER(DUMP_TYPE_TX_MIC);
        DEBUG_DATA_DUMP_UPDATE_HEADER_SEQ_NUM(DUMP_TYPE_TX_MIC);
        DEBUG_DATA_DUMP_BY_UART_DATA(data, len);
    }
    #endif

    int rval = 0;
    if (audio_tx_func)
    {
        rval = audio_tx_func(data, len);
    }
    else
    {
        AUDE_LOGE("send_audio_frame failed, invalid audio_tx_func\n");
        return 0;
    }

    if (rval < 0)
    {
        AUDE_LOGE("Failed to send audio data, reason: %d\n", rval);
        return 0;
    }
    else
    {
        AUDE_LOGD("record ret: %d\n", rval);
    }

    return len;
}

static bk_err_t aud_send_msg(void)
{
    bk_err_t ret;
    aud_tras_msg_t msg;

    msg.op = AUD_TRAS_TX_DATA;

    if (aud_msg_que)
    {
        ret = rtos_push_to_queue(&aud_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            AUDE_LOGD("audio send msg: AUD_TRAS_TX_DATA fail\n");
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}

uint16_t get_buf_frame_cnt(void)
{
    uint16_t buf_frame_cnt = MIC_FRAME_NUM;
    
    #if CONFIG_AUD_VAD_SUPPORT
    app_aud_para_t * aud_para = get_app_aud_cust_para();
    if(aud_para->aec_config_voice.vad_enable)
    {
        uint32_t enc_frame_ms = bk_aud_get_enc_frame_len_in_ms();
        buf_frame_cnt = (aud_para->aec_config_voice.vad_start_threshold + enc_frame_ms/2)/enc_frame_ms;
        buf_frame_cnt = (buf_frame_cnt > MIC_FRAME_NUM)?buf_frame_cnt:MIC_FRAME_NUM;

        AUDE_LOGD("buf_frame_cnt:%d,vad_start_th:%d,enc_frame_ms:%d\n",
             buf_frame_cnt,
             aud_para->aec_config_voice.vad_start_threshold,
             enc_frame_ms);
    }
    else
    {
         AUDE_LOGD("buf_frame_cnt:%d\n",buf_frame_cnt);
    }    
    #endif

    return buf_frame_cnt;
}

int send_audio_data_to_net_transfer(uint8_t *data, unsigned int len)
{
#if CONFIG_AUD_VAD_SUPPORT
    app_aud_para_t * aud_para = get_app_aud_cust_para();
#endif

#if CONFIG_AUD_INTF_SUPPORT_OPUS
    uint16_t pkt_len = len;

    if ((ring_buffer_get_free_size(&mic_data_rb) >= len) && (ring_buffer_get_free_size(&mic_data_len_rb) >= sizeof(uint16_t)))
    {
        ring_buffer_write(&mic_data_rb, data, len);
        ring_buffer_write(&mic_data_len_rb, (uint8_t *)&pkt_len, sizeof(uint16_t));
        #if CONFIG_AUD_VAD_SUPPORT
        if(aud_para->aec_config_voice.vad_enable)
        {            
            if(VAD_SPEECH_START != bk_aud_intf_get_aec_vad_flag())//vad no speech detected
            {
                //buffer data is more than vad start threshold,dorp old data
                if((ring_buffer_get_fill_size(&mic_data_len_rb)/sizeof(uint16_t)) >= mic_tx_buf_frame_num)
                {              
                    ring_buffer_read(&mic_data_len_rb, (uint8_t *)&pkt_len, sizeof(uint16_t));
                    ring_buffer_read(&mic_data_rb, (uint8_t *)mic_drop_data, pkt_len);
                }
            }
            else
            {
                aud_send_msg();
                
            }
        }
        else
        #endif
        {
            aud_send_msg();
        }
    }
    else
    {
        AUDE_LOGE("len:%d,free size of mic_data_rb:%d or mic_data_len_rb:%d is not enough!\n",
             len,
             ring_buffer_get_free_size(&mic_data_rb),
             ring_buffer_get_free_size(&mic_data_len_rb));
        return 0;
    }
    
    return len;
#else
    #if CONFIG_AUD_VAD_SUPPORT
    uint32_t buf_fill_th = bk_aud_get_enc_output_size_in_byte();
    #if (CONFIG_G722_CODEC_RUN_ON_CPU0)
    buf_fill_th = bk_aud_get_enc_input_size_in_byte();
    #endif
    #endif

    if (ring_buffer_get_free_size(&mic_data_rb) >= len)
    {
        ring_buffer_write(&mic_data_rb, data, len);
        #if CONFIG_AUD_VAD_SUPPORT
        if(aud_para->aec_config_voice.vad_enable)
        {            
            if(VAD_SPEECH_START != bk_aud_intf_get_aec_vad_flag())//vad no speech detected
            {
                //buffer data is more than vad start threshold,dorp old data
                if(ring_buffer_get_fill_size(&mic_data_rb) >= mic_tx_buf_frame_num*buf_fill_th)
                {              
                    ring_buffer_read(&mic_data_rb, (uint8_t *)mic_drop_data, buf_fill_th);
                }
            }
            else
            {
                aud_send_msg();
            }
        }
        else
        #endif
        {
            aud_send_msg();
        } 
    }
    else
    {
        AUDE_LOGD("len:%d,free size of mic_data_rb:%d is not enough!\n",
             len,
             ring_buffer_get_free_size(&mic_data_rb));
        return 0;
    }

    return len;
#endif

}

static void aud_tras_main(void)
{
    bk_err_t ret = BK_OK;
    GLOBAL_INT_DECLARATION();
    int32_t size = 0;
    uint32_t count = 0;

    uint8_t *mic_temp_buff = NULL;
    uint32_t buf_fill_th = bk_aud_get_enc_output_size_in_byte();
    #if (CONFIG_G722_CODEC_RUN_ON_CPU0)
    buf_fill_th = bk_aud_get_enc_input_size_in_byte();
    #endif
    #if CONFIG_AUD_INTF_SUPPORT_OPUS
    uint16_t pkt_len = 0;
    int32_t len_buf_size = 0;
    #endif

    #if 0
    if (tx_mic_data_flag)
    {
        TX_MIC_DATA_DUMP_OPEN();
    }
    #endif

    rtos_set_semaphore(&aud_sem);
#if CONFIG_PSRAM
    mic_temp_buff = psram_malloc(buf_fill_th);
#else
    mic_temp_buff = os_malloc(buf_fill_th);
#endif
    if (NULL == mic_temp_buff)
    {
        AUDE_LOGE("mic_temp_buff malloc fail\n");
        goto aud_tras_exit;
    }

    while (1)
    {
        aud_tras_msg_t msg;

        ret = rtos_pop_from_queue(&aud_msg_que, &msg, BEKEN_WAIT_FOREVER);
        if (kNoErr == ret)
        {
            switch (msg.op)
            {
                case AUD_TRAS_TX_DATA:
                    #if CONFIG_AUD_INTF_SUPPORT_OPUS
                    len_buf_size = ring_buffer_get_fill_size(&mic_data_len_rb);
                    
                    while(sizeof(uint16_t) <= len_buf_size)
                    {
                        GLOBAL_INT_DISABLE();
                        count = ring_buffer_read(&mic_data_len_rb, (uint8_t *)&pkt_len, sizeof(uint16_t));
                        if(count == sizeof(uint16_t))
                        {
                            size = ring_buffer_get_fill_size(&mic_data_rb);
                            if (size >= pkt_len)
                            {
                                count = ring_buffer_read(&mic_data_rb, mic_temp_buff, pkt_len);
                            }
                            else
                            {
                                AUDE_LOGE("mic_data_rb fill size (%d) < pkt_len(%d)\n", size, pkt_len);
                            }
                        }
                        else
                        {
                            AUDE_LOGE("mic_data_len_rb read count (%d) != sizeof(uint16_t):%d\n", size, sizeof(uint16_t));
                        }
                        GLOBAL_INT_RESTORE();

                        if (count == pkt_len)
                        {
                            send_audio_frame(mic_temp_buff, pkt_len);
                            #if CONFIG_AUD_VAD_SUPPORT
                            aud_tras_update_tx_size(pkt_len);
                            #endif
                        }
                        else
                        {
                            AUDE_LOGE("mic_data_rb read count(%d) != pkt_len(%d)\n", count, pkt_len);
                        }
                        len_buf_size -= sizeof(uint16_t);
                        rtos_delay_milliseconds(5);
                    }
                    #else
                    size = ring_buffer_get_fill_size(&mic_data_rb);
                    while(size >= buf_fill_th)
                    {
                        GLOBAL_INT_DISABLE();
                        count = ring_buffer_read(&mic_data_rb, mic_temp_buff, buf_fill_th);
                        GLOBAL_INT_RESTORE();

                        if (count == buf_fill_th)
                        {
                            send_audio_frame(mic_temp_buff, buf_fill_th);
                            #if CONFIG_AUD_VAD_SUPPORT
                            aud_tras_update_tx_size(buf_fill_th);
                            #endif
                        }
                        else
                        {
                            AUDE_LOGE("mic_data_rb read count(%d) != pkt_len(%d)\n", count, buf_fill_th);
                        }
                        size -= buf_fill_th;
                        rtos_delay_milliseconds(5);
                    }
                    #endif
                    break;

                case AUD_TRAS_EXIT:
                    AUDE_LOGD("goto: AUD_TRAS_EXIT\n");
                    goto aud_tras_exit;
                    break;

                default:
                    break;
            }
        }
    }

aud_tras_exit:

    if (mic_temp_buff)
    {
#if CONFIG_PSRAM
        psram_free(mic_temp_buff);
#else
        os_free(mic_temp_buff);
#endif
    }

    #if 0
    if (tx_mic_data_flag)
    {
        TX_MIC_DATA_DUMP_CLOSE();
    }
    #endif

    if (mic_data_buffer)
    {
        ring_buffer_clear(&mic_data_rb);
#if CONFIG_PSRAM
        psram_free(mic_data_buffer);
#else
        os_free(mic_data_buffer);
#endif
        mic_data_buffer = NULL;
    }

    #if CONFIG_AUD_INTF_SUPPORT_OPUS
    if (mic_data_len_buffer)
    {
        ring_buffer_clear(&mic_data_len_rb);
        psram_free(mic_data_len_buffer);
        mic_data_len_buffer = NULL;
    }
    #endif

    #if CONFIG_AUD_VAD_SUPPORT
    if (mic_drop_data)
    {
        psram_free(mic_drop_data);
    }
    #endif

    /* delete msg queue */
    ret = rtos_deinit_queue(&aud_msg_que);
    if (ret != kNoErr)
    {
        AUDE_LOGE("delete message queue fail\n");
    }
    aud_msg_que = NULL;

    /* delete task */
    aud_thread_hdl = NULL;

    AUDE_LOGI("delete audio transfer task\n");

    rtos_set_semaphore(&aud_sem);

    rtos_delete_thread(NULL);

    mic_tx_buf_frame_num = MIC_FRAME_NUM;
}
void audio_tras_register_tx_data_func(user_audio_tx_data_func func)
{
    audio_tx_func = func;
}
bk_err_t audio_tras_init(void)
{
    bk_err_t ret = BK_OK;
    uint16_t buf_frame_num;
    uint32_t tx_trans_buf_size;

    mic_tx_buf_frame_num = get_buf_frame_cnt();
    buf_frame_num = mic_tx_buf_frame_num + PRE_VAD_START_FRAME_NUM;
    tx_trans_buf_size = bk_aud_get_enc_output_size_in_byte()*(buf_frame_num);//2 more frame than vad start threshold

    #if (CONFIG_G722_CODEC_RUN_ON_CPU0)
    tx_trans_buf_size = bk_aud_get_enc_input_size_in_byte()*(buf_frame_num);
    #endif
#if CONFIG_PSRAM
    mic_data_buffer = psram_malloc(tx_trans_buf_size);
#else
    mic_data_buffer = os_malloc(tx_trans_buf_size);
#endif
    if (mic_data_buffer == NULL)
    {
        AUDE_LOGE("malloc mic_data_buffer fail\n");
        return BK_FAIL;
    }
    ring_buffer_init(&mic_data_rb, mic_data_buffer, tx_trans_buf_size, DMA_ID_MAX, RB_DMA_TYPE_NULL);

    #if CONFIG_AUD_INTF_SUPPORT_OPUS
    mic_data_len_buffer = psram_malloc(sizeof(uint16_t)*(buf_frame_num));
    if (mic_data_len_buffer == NULL)
    {
        AUDE_LOGE("malloc mic_data_buffer fail\n");
        return BK_FAIL;
    }
    ring_buffer_init(&mic_data_len_rb, mic_data_len_buffer, sizeof(uint16_t)*(buf_frame_num), DMA_ID_MAX, RB_DMA_TYPE_NULL);
    #endif

    #if CONFIG_AUD_VAD_SUPPORT
    #if (CONFIG_G722_CODEC_RUN_ON_CPU0)
    mic_drop_data = psram_malloc(bk_aud_get_enc_input_size_in_byte());
    #else
    mic_drop_data = psram_malloc(bk_aud_get_enc_output_size_in_byte());
    #endif
    if (NULL == mic_drop_data)
    {
        AUDE_LOGE("mic_drop_data malloc fail\n");
    }
    #endif

    ret = rtos_init_semaphore(&aud_sem, 1);
    if (ret != BK_OK)
    {
        AUDE_LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_init_queue(&aud_msg_que,
                          "tras_que",
                          sizeof(aud_tras_msg_t),
                          ((buf_frame_num>20)?buf_frame_num:20));
    if (ret != kNoErr)
    {
        AUDE_LOGE("create agoar audio tras message queue fail\n");
        goto fail;
    }
    AUDE_LOGI("create agoar audio tras message queue complete\n");

    /* create task to asr */
    ret = rtos_create_thread(&aud_thread_hdl,
                             3,
                             "audio_tras",
                             (beken_thread_function_t)aud_tras_main,
#if CONFIG_LINGXIN_AI_EN
                             2048*2,
#else
                             2048,
#endif
                             NULL);
    if (ret != kNoErr)
    {
        AUDE_LOGE("create audio transfer driver task fail\n");
        aud_thread_hdl = NULL;
        goto fail;
    }

    rtos_get_semaphore(&aud_sem, BEKEN_NEVER_TIMEOUT);

    AUDE_LOGI("create audio transfer driver task complete\n");

    return BK_OK;

fail:
    if (mic_data_buffer)
    {
        ring_buffer_clear(&mic_data_rb);
#if CONFIG_PSRAM
        psram_free(mic_data_buffer);
#else
        os_free(mic_data_buffer);
#endif
        mic_data_buffer = NULL;
    }

    #if CONFIG_AUD_INTF_SUPPORT_OPUS
    if (mic_data_len_buffer)
    {
        ring_buffer_clear(&mic_data_len_rb);
        psram_free(mic_data_len_buffer);
        mic_data_len_buffer = NULL;
    }
    #endif

    #if CONFIG_AUD_VAD_SUPPORT
    if (mic_drop_data)
    {
        psram_free(mic_drop_data);
    }
    #endif

    if (aud_sem)
    {
        rtos_deinit_semaphore(&aud_sem);
        aud_sem = NULL;
    }

    if (aud_msg_que)
    {
        rtos_deinit_queue(&aud_msg_que);
        aud_msg_que = NULL;
    }

    mic_tx_buf_frame_num = MIC_FRAME_NUM;

    return BK_FAIL;
}

bk_err_t audio_tras_deinit(void)
{
    bk_err_t ret;
    aud_tras_msg_t msg;

    msg.op = AUD_TRAS_EXIT;
    if (aud_msg_que)
    {
        ret = rtos_push_to_queue_front(&aud_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            AUDE_LOGE("audio send msg: AUD_TRAS_EXIT fail\n");
            return BK_FAIL;
        }

        rtos_get_semaphore(&aud_sem, BEKEN_NEVER_TIMEOUT);

        rtos_deinit_semaphore(&aud_sem);
        aud_sem = NULL;
    }

    return BK_OK;
}


