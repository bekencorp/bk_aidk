#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>

#include <driver/audio_ring_buff.h>
#include "audio_transfer.h"
#include "volc_rtc.h"
#include "volc_config.h"


#define TAG "volc_tras"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
#endif


typedef enum
{
    AUD_TRAS_TX_DATA = 0,

    AUD_TRAS_EXIT,
} aud_tras_op_t;

typedef struct
{
    aud_tras_op_t op;
    uint16_t len;
} aud_tras_msg_t;

static beken_thread_t  byte_aud_thread_hdl = NULL;
static beken_queue_t byte_aud_msg_que = NULL;
static beken_semaphore_t byte_aud_sem = NULL;
static RingBufferContext mic_data_rb;
static uint8_t *mic_data_buffer = NULL;
#if defined(CONFIG_USE_G722_CODEC)  //G722
#if (CONFIG_G722_CODEC_RUN_ON_CPU1)
#define MIC_FRAME_SIZE   (160)
#define SEND_FRAME_SIZE   MIC_FRAME_SIZE
#endif
#if (CONFIG_G722_CODEC_RUN_ON_CPU0)
#define MIC_FRAME_SIZE   (640)
#define SEND_FRAME_SIZE   (MIC_FRAME_SIZE * (CONFIG_AUDIO_FRAME_DURATION_MS / 20))
#endif
//#elif defined(CONFIG_USE_G711U_CODEC) || defined(CONFIG_USE_G711A_CODEC)
//#define MIC_FRAME_SIZE     160
#elif defined(CONFIG_USE_OPUS_CODEC)  // OPUS
#define MIC_FRAME_SIZE   320
#define SEND_FRAME_SIZE   MIC_FRAME_SIZE
#else
#define MIC_FRAME_SIZE   160
#define SEND_FRAME_SIZE   MIC_FRAME_SIZE
#endif
#define MIC_FRAME_NUM 4

extern bool agoora_tx_mic_data_flag;
extern bool g_connected_flag;


static int send_byte_audio_frame(uint8_t *data, unsigned int len)
{
    audio_frame_info_t info = { 0 };

    if (!g_connected_flag)
    {
        return 0;
    }

#ifdef CONFIG_USE_G722_CODEC
    #if (CONFIG_G722_CODEC_RUN_ON_CPU1)
    info.data_type = AUDIO_DATA_TYPE_G722;
    #endif
    #if (CONFIG_G722_CODEC_RUN_ON_CPU0)
    info.data_type = AUDIO_DATA_TYPE_PCM;
    #endif
#elif CONFIG_USE_OPUS_CODEC
    info.data_type = AUDIO_DATA_TYPE_OPUS;
#else
    info.data_type = AUDIO_DATA_TYPE_PCMA;
#endif

    #if CONFIG_DEBUG_DUMP
    if (agoora_tx_mic_data_flag)
    {
        //BYTE_TX_MIC_DATA_DUMP_DATA(data, len);
        #if 0
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_NUM(DUMP_TYPE_BYTE_TX_MIC,1);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW(DUMP_TYPE_BYTE_TX_MIC,0,DUMP_FILE_TYPE_G722,len);
        #else
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_BYTE_TX_MIC,0,len);
        #endif
        DEBUG_DATA_DUMP_UPDATE_HEADER_TIMESTAMP(DUMP_TYPE_BYTE_TX_MIC);
        DEBUG_DATA_DUMP_BY_UART_HEADER(DUMP_TYPE_BYTE_TX_MIC);
        DEBUG_DATA_DUMP_UPDATE_HEADER_SEQ_NUM(DUMP_TYPE_BYTE_TX_MIC);
        DEBUG_DATA_DUMP_BY_UART_DATA(data, len);
    }
    #endif

    int rval = bk_byte_rtc_audio_data_send(data, len, &info);
    if (rval < 0)
    {
        LOGE("Failed to send audio data, reason: %s\n", byte_rtc_err_2_str(rval));
        return 0;
    }
    else
    {
        LOGD("record ret: %d\n", rval);
    }

    return len;
}


#if CONFIG_USE_OPUS_CODEC
static bk_err_t send_audio_msg(uint16_t len)
{
    bk_err_t ret;
    aud_tras_msg_t msg;

    msg.op = AUD_TRAS_TX_DATA;
    msg.len = len;

    //LOGE("audio send msg: len:%d\n", len);
    if (byte_aud_msg_que)
    {
        ret = rtos_push_to_queue(&byte_aud_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("audio send msg: AUD_TRAS_TX_DATA fail\n");
            return kOverrunErr;
        }

        return ret;
    }
    else
    {
        LOGE("send_audio_msg:byte_aud_msg_que is NULL\n");
    }
    return kNoResourcesErr;
}
#else
static bk_err_t send_audio_msg(void)
{
    bk_err_t ret;
    aud_tras_msg_t msg;

    msg.op = AUD_TRAS_TX_DATA;

    if (byte_aud_msg_que)
    {
        ret = rtos_push_to_queue(&byte_aud_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("audio send msg: AUD_TRAS_TX_DATA fail\n");
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}
#endif

int send_audio_data_to_byte(uint8_t *data, unsigned int len)
{
    #if CONFIG_USE_OPUS_CODEC
    //LOGI("send audio, len:%d\n",len);
    if (ring_buffer_get_free_size(&mic_data_rb) >= len)
    {
        ring_buffer_write(&mic_data_rb, data, len);
        send_audio_msg(len);

        uint32_t fill_size = ring_buffer_get_fill_size(&mic_data_rb);
        LOGD("len:%d,mic_data_rb:fill size:%d\n",len,fill_size);

        //BK_ASSERT(len == fill_size);
    }
    else
    {
        LOGE("len:%d,mic_data_rb fill size:%d,free size:%d,not enough\n",
            len,
            ring_buffer_get_fill_size(&mic_data_rb),
            ring_buffer_get_free_size(&mic_data_rb));
        return 0;
    }
    #else
    if (ring_buffer_get_free_size(&mic_data_rb) >= len)
    {
        ring_buffer_write(&mic_data_rb, data, len);
    }
    else
    {
        return 0;
    }

    if (ring_buffer_get_fill_size(&mic_data_rb) >= MIC_FRAME_SIZE)
    {
        send_audio_msg();
    }
    #endif

    return len;
}

static void byte_aud_tras_main(void)
{
    bk_err_t ret = BK_OK;
    GLOBAL_INT_DECLARATION();
    uint32_t size = 0;
    uint32_t count = 0;

    uint8_t *mic_temp_buff = NULL;

    #if 0
    if (agoora_tx_mic_data_flag)
    {
        BYTE_TX_MIC_DATA_DUMP_OPEN();
    }
    #endif

    rtos_set_semaphore(&byte_aud_sem);

    mic_temp_buff = psram_malloc(SEND_FRAME_SIZE);
    if (NULL == mic_temp_buff)
    {
        LOGE("mic_temp_buff malloc fail\n");
        goto aud_tras_exit;
    }

    while (1)
    {
        aud_tras_msg_t msg;

        ret = rtos_pop_from_queue(&byte_aud_msg_que, &msg, BEKEN_WAIT_FOREVER);
        if (kNoErr == ret)
        {
            switch (msg.op)
            {
                case AUD_TRAS_TX_DATA:
                    #if CONFIG_USE_OPUS_CODEC
                    size = ring_buffer_get_fill_size(&mic_data_rb);
                    //LOGI("rb get, size:%d, msg len:%d\n",size, msg.len);
                    if (size >= msg.len)
                    {
                        mic_temp_buff = psram_malloc(MIC_FRAME_SIZE);
                        if (mic_temp_buff != NULL)
                        {
                            GLOBAL_INT_DISABLE();
                            count = ring_buffer_read(&mic_data_rb, mic_temp_buff, msg.len);
                            GLOBAL_INT_RESTORE();
                            //LOGI("msg len:%d, count size:%d\n",msg.len, count);
                            if (count == msg.len)
                            {
                                //LOGE("saf in!\n");   
                                send_byte_audio_frame(mic_temp_buff, count);
                                //LOGE("saf out\n", count,msg.len);
                            }
                            else
                            {
                                LOGE("mic_data_rb count(%d) != frm size:%d\n", count,msg.len);
                            }
                            psram_free(mic_temp_buff);
                        }
                        else
                        {
                            LOGE("mic_temp_buff alloc fail!\n");
                        }
                    }
                    #else
                    size = ring_buffer_get_fill_size(&mic_data_rb);
                    if (size >= MIC_FRAME_SIZE)
                    {
                        mic_temp_buff = psram_malloc(MIC_FRAME_SIZE);
                        if (mic_temp_buff != NULL)
                        {
                            GLOBAL_INT_DISABLE();
                            count = ring_buffer_read(&mic_data_rb, mic_temp_buff, MIC_FRAME_SIZE);
                            GLOBAL_INT_RESTORE();
                            if (count == MIC_FRAME_SIZE)
                            {
                                send_byte_audio_frame(mic_temp_buff, count);
                            }
                            else
                            {
                                LOGD("ring_buffer_read count(%d) != MIC_FRAME_SIZE(160)\n", count);
                            }
                            psram_free(mic_temp_buff);
                        }

                        rtos_delay_milliseconds(2);
                        send_audio_msg();
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
        BYTE_TX_MIC_DATA_DUMP_CLOSE();
    }
    #endif

    if (mic_data_buffer)
    {
        ring_buffer_clear(&mic_data_rb);
        psram_free(mic_data_buffer);
        mic_data_buffer = NULL;
    }

    /* delete msg queue */
    ret = rtos_deinit_queue(&byte_aud_msg_que);
    if (ret != kNoErr)
    {
        LOGE("delete message queue fail\n");
    }
    byte_aud_msg_que = NULL;

    /* delete task */
    byte_aud_thread_hdl = NULL;

    LOGI("delete byte audio transfer task\n");

    rtos_set_semaphore(&byte_aud_sem);

    rtos_delete_thread(NULL);
}

bk_err_t audio_tras_init(void)
{
    bk_err_t ret = BK_OK;

    mic_data_buffer = psram_malloc(MIC_FRAME_SIZE * MIC_FRAME_NUM);
    if (mic_data_buffer == NULL)
    {
        LOGE("malloc mic_data_buffer fail\n");
        return BK_FAIL;
    }
    ring_buffer_init(&mic_data_rb, mic_data_buffer, MIC_FRAME_SIZE * MIC_FRAME_NUM, DMA_ID_MAX, RB_DMA_TYPE_NULL);

    ret = rtos_init_semaphore(&byte_aud_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_init_queue(&byte_aud_msg_que,
                          "byte_tras_que",
                          sizeof(aud_tras_msg_t),
                          20);
    if (ret != kNoErr)
    {
        LOGE("create agoar audio tras message queue fail\n");
        goto fail;
    }
    LOGI("create agoar audio tras message queue complete\n");

    /* create task to asr */
    ret = rtos_create_thread(&byte_aud_thread_hdl,
                             4,
                             "byte_audio_tras",
                             (beken_thread_function_t)byte_aud_tras_main,
                             2048,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("create audio transfer driver task fail\n");
        byte_aud_thread_hdl = NULL;
        goto fail;
    }

    rtos_get_semaphore(&byte_aud_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("create audio transfer driver task complete\n");

    return BK_OK;

fail:
    if (mic_data_buffer)
    {
        ring_buffer_clear(&mic_data_rb);
        psram_free(mic_data_buffer);
        mic_data_buffer = NULL;
    }

    if (byte_aud_sem)
    {
        rtos_deinit_semaphore(&byte_aud_sem);
        byte_aud_sem = NULL;
    }

    if (byte_aud_msg_que)
    {
        rtos_deinit_queue(&byte_aud_msg_que);
        byte_aud_msg_que = NULL;
    }

    return BK_FAIL;
}

bk_err_t audio_tras_deinit(void)
{
    bk_err_t ret;
    aud_tras_msg_t msg;

    msg.op = AUD_TRAS_EXIT;
    if (byte_aud_msg_que)
    {
        ret = rtos_push_to_queue_front(&byte_aud_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("audio send msg: AUD_TRAS_EXIT fail\n");
            return BK_FAIL;
        }

        rtos_get_semaphore(&byte_aud_sem, BEKEN_NEVER_TIMEOUT);

        rtos_deinit_semaphore(&byte_aud_sem);
        byte_aud_sem = NULL;
    }

    return BK_OK;
}


