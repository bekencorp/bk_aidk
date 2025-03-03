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


#define TAG "agora_tras"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#define AGORA_TX_MIC_DATA_DUMP

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
static RingBufferContext *mic_data_rb = NULL;
#if defined(CONFIG_USE_G722_CODEC)
#define MIC_FRAME_SIZE   160
#elif defined(CONFIG_USE_G711U_CODEC) || defined(CONFIG_USE_G711A_CODEC)
#define MIC_FRAME_SIZE   320
#else
#define MIC_FRAME_SIZE   160
#endif
#define MIC_FRAME_NUM 4

extern bool agoora_tx_mic_data_flag;


static int send_agora_audio_frame(uint8_t *data, unsigned int len)
{
    audio_frame_info_t info = { 0 };

#ifdef CONFIG_USE_G722_CODEC
    info.data_type = AUDIO_DATA_TYPE_G722;
#else
    info.data_type = AUDIO_DATA_TYPE_PCMA;
#endif

    if (agoora_tx_mic_data_flag)
    {
        AGORA_TX_MIC_DATA_DUMP_DATA(data, len);
    }

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
            LOGE("audio send msg: AUD_TRAS_TX_DATA fail \r\n");
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}

static void agora_aud_tras_main(void)
{
    bk_err_t ret = BK_OK;
    uint32_t size = 0;
    uint32_t count = 0;

    uint8_t *mic_temp_buff = NULL;

    if (agoora_tx_mic_data_flag)
    {
        AGORA_TX_MIC_DATA_DUMP_OPEN();
    }

    mic_temp_buff = psram_malloc(MIC_FRAME_SIZE);
    if (mic_temp_buff == NULL)
    {
        LOGE("psram_malloc mic_temp_buff: %d fail\n", MIC_FRAME_SIZE);
    }

    rtos_set_semaphore(&agora_aud_sem);

    agora_aud_send_msg();

    while (1)
    {
        aud_tras_msg_t msg;
        ret = rtos_pop_from_queue(&agora_aud_msg_que, &msg, BEKEN_WAIT_FOREVER);
        if (kNoErr == ret)
        {
            switch (msg.op)
            {
                case AUD_TRAS_TX_DATA:
                    size = ring_buffer_get_fill_size(mic_data_rb);
                    if (size >= MIC_FRAME_SIZE)
                    {
                        count = ring_buffer_read(mic_data_rb, mic_temp_buff, MIC_FRAME_SIZE);
                        if (count == MIC_FRAME_SIZE)
                        {
                            send_agora_audio_frame(mic_temp_buff, count);
                        }
                        else
                        {
                            LOGE("ring_buffer_read count(%d) != MIC_FRAME_SIZE(160) \r\n", count);
                        }
                    }
                    else
                    {
                        rtos_delay_milliseconds(5);
                    }
                    agora_aud_send_msg();
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

    if (agoora_tx_mic_data_flag)
    {
        AGORA_TX_MIC_DATA_DUMP_CLOSE();
    }

    psram_free(mic_temp_buff);

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
}

bk_err_t audio_tras_init(RingBufferContext *mic_rb)
{
    bk_err_t ret = BK_OK;

    if (!mic_rb)
    {
        LOGE("%s, %d, mic_rb is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

    /* save mic_rb */
    mic_data_rb = mic_rb;

    ret = rtos_init_semaphore(&agora_aud_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_init_queue(&agora_aud_msg_que,
                          "aud_int_que",
                          sizeof(aud_tras_msg_t),
                          60);
    if (ret != kNoErr)
    {
        LOGE("create agoar audio tras message queue fail \r\n");
        goto fail;
    }
    LOGI("create audio asr message queue complete \r\n");

    /* create task to asr */
    ret = rtos_create_thread(&agora_aud_thread_hdl,
                             6,
                             "agora_audio_tras",
                             (beken_thread_function_t)agora_aud_tras_main,
                             2048,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("create audio transfer driver task fail \r\n");
        agora_aud_thread_hdl = NULL;
        goto fail;
    }

    rtos_get_semaphore(&agora_aud_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("create audio transfer driver task complete \r\n");

    return BK_OK;

fail:

    mic_data_rb = NULL;

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
            return kOverrunErr;
        }

        rtos_get_semaphore(&agora_aud_sem, BEKEN_NEVER_TIMEOUT);

        rtos_deinit_semaphore(&agora_aud_sem);
        agora_aud_sem = NULL;
    }

    return BK_OK;
}


