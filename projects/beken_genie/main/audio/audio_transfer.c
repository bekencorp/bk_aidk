#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>

#include <driver/audio_ring_buff.h>
#include "audio_transfer.h"
#include "agora_rtc.h"
#include "agora_config.h"
#include "aud_intf.h"
#include <modules/audio_process.h>

#define TAG "agora_tras"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
#endif

//#define AGORA_TX_MIC_DATA_DUMP

#ifdef AGORA_TX_MIC_DATA_DUMP
#include "uart_util.h"
static uart_util_t g_agora_mic_uart_util = {0};
#define AGORA_TX_MIC_DATA_DUMP_UART_ID            (1)
#define AGORA_TX_MIC_DATA_DUMP_UART_BAUD_RATE     (2000000)

#define AGORA_TX_MIC_DATA_DUMP_OPEN()                        uart_util_create(&g_agora_mic_uart_util, AGORA_TX_MIC_DATA_DUMP_UART_ID, AGORA_TX_MIC_DATA_DUMP_UART_BAUD_RATE)
#define AGORA_TX_MIC_DATA_DUMP_CLOSE()                       uart_util_destroy(&g_agora_mic_uart_util)
#define AGORA_TX_MIC_DATA_DUMP_DATA(data_buf, len)           uart_util_tx_data(&g_agora_mic_uart_util, data_buf, len)
#else
#define AGORA_TX_MIC_DATA_DUMP_OPEN()
#define AGORA_TX_MIC_DATA_DUMP_CLOSE()
#define AGORA_TX_MIC_DATA_DUMP_DATA(data_buf, len)
#endif  //AGORA_TX_MIC_DATA_DUMP


typedef enum
{
    AUD_TRAS_TX_DATA = 0,

    AUD_TRAS_EXIT,
} aud_tras_op_t;

typedef struct
{
    aud_tras_op_t op;
} aud_tras_msg_t;

static beken_thread_t  agora_aud_thread_hdl = NULL;
static beken_queue_t agora_aud_msg_que = NULL;
static beken_semaphore_t agora_aud_sem = NULL;
static RingBufferContext mic_data_rb;
static uint8_t *mic_data_buffer = NULL;
#if CONFIG_AUD_VAD_SUPPORT
static uint8_t *mic_drop_data = NULL;
#endif

#define MIC_FRAME_NUM 4
#define PRE_VAD_START_FRAME_NUM 2 
#if CONFIG_AUD_INTF_SUPPORT_OPUS
static RingBufferContext mic_data_len_rb;
static uint8_t *mic_data_len_buffer = NULL;
#endif
static uint16_t mic_tx_buf_frame_num = MIC_FRAME_NUM;

extern bool agoora_tx_mic_data_flag;
extern bool g_connected_flag;

enum vad_state
{
    VAD_NONE              = (0x00),
    VAD_SPEECH_START      = (0x01),
    VAD_SPEECH_END        = (0x02),
};

uint8_t agora_aud_type_mapping(uint8_t codec_type)
{
    uint8_t aud_data_type = AUDIO_DATA_TYPE_GENERIC;
    switch(codec_type)
    {
        case AUD_INTF_VOC_DATA_TYPE_G711A:
        case AUD_INTF_VOC_DATA_TYPE_G711U:
        {
            LOGE("%s unsupported codec type:%d \r\n", __func__,codec_type);
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_PCM:
        {
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_G722:
        {
            aud_data_type = AUDIO_DATA_TYPE_G722;
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_OPUS:
        {
            aud_data_type = AUDIO_DATA_TYPE_OPUS;
            break;
        }
        default:
        {
            LOGE("%s unknown codec type:%d \r\n", __func__,codec_type);
            break;
        }
    }

    return aud_data_type;
}

static int send_agora_audio_frame(uint8_t *data, unsigned int len)
{
    audio_frame_info_t info = { 0 };

    if (!g_connected_flag)
    {
        return 0;
    }

    info.data_type = agora_aud_type_mapping(bk_aud_get_encoder_type());


    #if CONFIG_DEBUG_DUMP
    if (agoora_tx_mic_data_flag)
    {
        //AGORA_TX_MIC_DATA_DUMP_DATA(data, len);
        #if 0
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_NUM(DUMP_TYPE_TX_MIC,1);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW(DUMP_TYPE_TX_MIC,0,DUMP_FILE_TYPE_G722,len);
        #else
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_TX_MIC,0,len);
        #endif
        DEBUG_DATA_DUMP_UPDATE_HEADER_TIMESTAMP(DUMP_TYPE_TX_MIC);
        DEBUG_DATA_DUMP_BY_UART_HEADER(DUMP_TYPE_TX_MIC);
        DEBUG_DATA_DUMP_UPDATE_HEADER_SEQ_NUM(DUMP_TYPE_TX_MIC);
        DEBUG_DATA_DUMP_BY_UART_DATA(data, len);
    }
    #endif

    int rval = bk_agora_rtc_audio_data_send(data, len, &info);
    if (rval < 0)
    {
        LOGE("Failed to send audio data, reason: %s\n", agora_rtc_err_2_str(rval));
        return 0;
    }
    else
    {
        LOGD("record ret: %d\n", rval);
    }

    return len;
}

static bk_err_t agora_aud_send_msg(void)
{
    bk_err_t ret;
    aud_tras_msg_t msg;

    msg.op = AUD_TRAS_TX_DATA;

    if (agora_aud_msg_que)
    {
        ret = rtos_push_to_queue(&agora_aud_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGD("audio send msg: AUD_TRAS_TX_DATA fail\n");
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}

uint16_t agora_get_buf_frame_cnt(void)
{
    uint16_t buf_frame_cnt = MIC_FRAME_NUM;
    
    #if CONFIG_AUD_VAD_SUPPORT
    app_aud_para_t * aud_para = get_app_aud_cust_para();
    if(aud_para->aec_config_voice.vad_enable)
    {
        uint32_t enc_frame_ms = bk_aud_get_enc_frame_len_in_ms();
        buf_frame_cnt = (aud_para->aec_config_voice.vad_start_threshold + enc_frame_ms/2)/enc_frame_ms;
        buf_frame_cnt = (buf_frame_cnt > MIC_FRAME_NUM)?buf_frame_cnt:MIC_FRAME_NUM;

        LOGI("buf_frame_cnt:%d,vad_start_th:%d,enc_frame_ms:%d\n",
             buf_frame_cnt,
             aud_para->aec_config_voice.vad_start_threshold,
             enc_frame_ms);
    }
    else
    {
         LOGI("buf_frame_cnt:%d\n",buf_frame_cnt);
    }    
    #endif

    return buf_frame_cnt;
}

int send_audio_data_to_agora(uint8_t *data, unsigned int len)
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
                agora_aud_send_msg();
                
            }
        }
        else
        #endif
        {
            agora_aud_send_msg();
        }
    }
    else
    {
        LOGE("len:%d,free size of mic_data_rb:%d or mic_data_len_rb:%d is not enough!\n",
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
                agora_aud_send_msg();
            }
        }
        else
        #endif
        {
            agora_aud_send_msg();
        } 
    }
    else
    {
        LOGE("len:%d,free size of mic_data_rb:%d is not enough!\n",
             len,
             ring_buffer_get_free_size(&mic_data_rb));
        return 0;
    }

    return len;
#endif

}

static void agora_aud_tras_main(void)
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
    if (agoora_tx_mic_data_flag)
    {
        AGORA_TX_MIC_DATA_DUMP_OPEN();
    }
    #endif

    rtos_set_semaphore(&agora_aud_sem);

    mic_temp_buff = psram_malloc(buf_fill_th);
    if (NULL == mic_temp_buff)
    {
        LOGE("mic_temp_buff malloc fail\n");
        goto aud_tras_exit;
    }

    while (1)
    {
        aud_tras_msg_t msg;

        ret = rtos_pop_from_queue(&agora_aud_msg_que, &msg, BEKEN_WAIT_FOREVER);
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
                                LOGE("mic_data_rb fill size (%d) < pkt_len(%d)\n", size, pkt_len);
                            }
                        }
                        else
                        {
                            LOGE("mic_data_len_rb read count (%d) != sizeof(uint16_t):%d\n", size, sizeof(uint16_t));
                        }
                        GLOBAL_INT_RESTORE();

                        if (count == pkt_len)
                        {
                            send_agora_audio_frame(mic_temp_buff, pkt_len);
                            #if CONFIG_AUD_VAD_SUPPORT
                            aud_tras_update_tx_size(pkt_len);
                            #endif
                        }
                        else
                        {
                            LOGE("mic_data_rb read count(%d) != pkt_len(%d)\n", count, pkt_len);
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
                            send_agora_audio_frame(mic_temp_buff, buf_fill_th);
                            #if CONFIG_AUD_VAD_SUPPORT
                            aud_tras_update_tx_size(buf_fill_th);
                            #endif
                        }
                        else
                        {
                            LOGE("mic_data_rb read count(%d) != pkt_len(%d)\n", count, buf_fill_th);
                        }
                        size -= buf_fill_th;
                        rtos_delay_milliseconds(5);
                    }
                    #endif
                    break;

                case AUD_TRAS_EXIT:
                    LOGD("goto: AUD_TRAS_EXIT\n");
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
        psram_free(mic_temp_buff);
    }

    #if 0
    if (agoora_tx_mic_data_flag)
    {
        AGORA_TX_MIC_DATA_DUMP_CLOSE();
    }
    #endif

    if (mic_data_buffer)
    {
        ring_buffer_clear(&mic_data_rb);
        psram_free(mic_data_buffer);
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
    ret = rtos_deinit_queue(&agora_aud_msg_que);
    if (ret != kNoErr)
    {
        LOGE("delete message queue fail\n");
    }
    agora_aud_msg_que = NULL;

    /* delete task */
    agora_aud_thread_hdl = NULL;

    LOGI("delete agora audio transfer task\n");

    rtos_set_semaphore(&agora_aud_sem);

    rtos_delete_thread(NULL);

    mic_tx_buf_frame_num = MIC_FRAME_NUM;
}

bk_err_t audio_tras_init(void)
{
    bk_err_t ret = BK_OK;
    uint16_t buf_frame_num;
    uint32_t tx_trans_buf_size;

    mic_tx_buf_frame_num = agora_get_buf_frame_cnt();
    buf_frame_num = mic_tx_buf_frame_num + PRE_VAD_START_FRAME_NUM;
    tx_trans_buf_size = bk_aud_get_enc_output_size_in_byte()*(buf_frame_num);//2 more frame than vad start threshold

    #if (CONFIG_G722_CODEC_RUN_ON_CPU0)
    tx_trans_buf_size = bk_aud_get_enc_input_size_in_byte()*(buf_frame_num);
    #endif

    mic_data_buffer = psram_malloc(tx_trans_buf_size);
    if (mic_data_buffer == NULL)
    {
        LOGE("malloc mic_data_buffer fail\n");
        return BK_FAIL;
    }
    ring_buffer_init(&mic_data_rb, mic_data_buffer, tx_trans_buf_size, DMA_ID_MAX, RB_DMA_TYPE_NULL);

    #if CONFIG_AUD_INTF_SUPPORT_OPUS
    mic_data_len_buffer = psram_malloc(sizeof(uint16_t)*(buf_frame_num));
    if (mic_data_len_buffer == NULL)
    {
        LOGE("malloc mic_data_buffer fail\n");
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
        LOGE("mic_drop_data malloc fail\n");
    }
    #endif

    ret = rtos_init_semaphore(&agora_aud_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_init_queue(&agora_aud_msg_que,
                          "agora_tras_que",
                          sizeof(aud_tras_msg_t),
                          ((buf_frame_num>20)?buf_frame_num:20));
    if (ret != kNoErr)
    {
        LOGE("create agoar audio tras message queue fail\n");
        goto fail;
    }
    LOGI("create agoar audio tras message queue complete\n");

    /* create task to asr */
    ret = rtos_create_thread(&agora_aud_thread_hdl,
                             4,
                             "agora_audio_tras",
                             (beken_thread_function_t)agora_aud_tras_main,
                             2048,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("create audio transfer driver task fail\n");
        agora_aud_thread_hdl = NULL;
        goto fail;
    }

    rtos_get_semaphore(&agora_aud_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("create audio transfer driver task complete\n");

    return BK_OK;

fail:
    if (mic_data_buffer)
    {
        ring_buffer_clear(&mic_data_rb);
        psram_free(mic_data_buffer);
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

    if (agora_aud_sem)
    {
        rtos_deinit_semaphore(&agora_aud_sem);
        agora_aud_sem = NULL;
    }

    if (agora_aud_msg_que)
    {
        rtos_deinit_queue(&agora_aud_msg_que);
        agora_aud_msg_que = NULL;
    }

    mic_tx_buf_frame_num = MIC_FRAME_NUM;

    return BK_FAIL;
}

bk_err_t audio_tras_deinit(void)
{
    bk_err_t ret;
    aud_tras_msg_t msg;

    msg.op = AUD_TRAS_EXIT;
    if (agora_aud_msg_que)
    {
        ret = rtos_push_to_queue_front(&agora_aud_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("audio send msg: AUD_TRAS_EXIT fail\n");
            return BK_FAIL;
        }

        rtos_get_semaphore(&agora_aud_sem, BEKEN_NEVER_TIMEOUT);

        rtos_deinit_semaphore(&agora_aud_sem);
        agora_aud_sem = NULL;
    }

    return BK_OK;
}


