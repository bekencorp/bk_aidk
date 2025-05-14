#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>

#include <driver/audio_ring_buff.h>
#include "audio_transfer.h"
// #include "beken_rtc.h"
// #include "beken_config.h"
#include "default_rtc_api.h"
#include "aud_intf.h"

#define TAG "aud_tras"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
#endif

#ifdef TX_MIC_DATA_DUMP
#include "uart_util.h"
static uart_util_t g_mic_uart_util = {0};

#define TX_MIC_DATA_DUMP_UART_ID                        (1)
#define TX_MIC_DATA_DUMP_UART_BAUD_RATE                 (2000000)
#define TX_MIC_DATA_DUMP_OPEN()                         uart_util_create(&g_mic_uart_util, TX_MIC_DATA_DUMP_UART_ID, TX_MIC_DATA_DUMP_UART_BAUD_RATE)
#define TX_MIC_DATA_DUMP_CLOSE()                        uart_util_destroy(&g_mic_uart_util)
#define TX_MIC_DATA_DUMP_DATA(data_buf, len)      uart_util_tx_data(&g_mic_uart_util, data_buf, len)
#else
#define TX_MIC_DATA_DUMP_OPEN()
#define TX_MIC_DATA_DUMP_CLOSE()
#define TX_MIC_DATA_DUMP_DATA(data_buf, len)
#endif

#ifdef RX_SPK_DATA_DUMP
#include "uart_util.h"
static uart_util_t g_spk_uart_util = {0};
#define RX_SPK_DATA_DUMP_UART_ID                        (1)
#define RX_SPK_DATA_DUMP_UART_BAUD_RATE                 (2000000)
#define RX_SPK_DATA_DUMP_OPEN()                         uart_util_create(&g_spk_uart_util, RX_SPK_DATA_DUMP_UART_ID, RX_SPK_DATA_DUMP_UART_BAUD_RATE)
#define RX_SPK_DATA_DUMP_CLOSE()                        uart_util_destroy(&g_spk_uart_util)
#define RX_SPK_DATA_DUMP_DATA(data_buf, len)            uart_util_tx_data(&g_spk_uart_util, data_buf, len)
#else
#define RX_SPK_DATA_DUMP_OPEN()
#define RX_SPK_DATA_DUMP_CLOSE()
#define RX_SPK_DATA_DUMP_DATA(data_buf, len)
#endif


static beken_thread_t  aud_thread_hdl = NULL;
static beken_queue_t aud_msg_que = NULL;
static beken_semaphore_t aud_sem = NULL;
static RingBufferContext mic_data_rb;
static uint8_t *mic_data_buffer = NULL;
static user_audio_tx_data_func audio_tx_func = NULL;

bool tx_mic_data_flag = false;
bool rx_spk_data_flag = false;

extern uint32_t volume;   // volume level, not gain.
extern uint32_t g_volume_gain[SPK_VOLUME_LEVEL];
extern app_aud_para_t app_aud_cust_para;

extern bool g_connected_flag;

static int user_audio_rx_data_handle(unsigned char *data, unsigned int size, void *info)
{
    bk_err_t ret = BK_OK;

    #if CONFIG_DEBUG_DUMP
    if(rx_spk_data_flag)
    {
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_RX_SPK,0,size);
        DEBUG_DATA_DUMP_UPDATE_HEADER_TIMESTAMP(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_BY_UART_HEADER(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_UPDATE_HEADER_SEQ_NUM(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_BY_UART_DATA(data, size);
    }
    #endif//CONFIG_DEBUG_DUMP

    ret = bk_aud_intf_write_spk_data((uint8_t *)data, (uint32_t)size);
    if (ret != BK_OK)
    {
        LOGE("write spk data fail \r\n");
    }

    return ret;
}

static int send_audio_frame(uint8_t *data, unsigned int len)
{
    audio_frame_info_t info = { 0 };

    //LOGE("send_audio_frame g_connected_flag:%d\r\n",g_connected_flag);
    if (!g_connected_flag)
    {
        return 0;
    }

#if CONFIG_AUD_INTF_SUPPORT_G722
    #if (CONFIG_G722_CODEC_RUN_ON_CPU1)
    info.data_type = AUDIO_DATA_TYPE_G722;
    #endif
    #if (CONFIG_G722_CODEC_RUN_ON_CPU0)
    info.data_type = AUDIO_DATA_TYPE_PCM;
    #endif
#elif CONFIG_AUD_INTF_SUPPORT_OPUS
    info.data_type = AUDIO_DATA_TYPE_OPUS;
#else
    info.data_type = AUDIO_DATA_TYPE_PCMA;
#endif

    #if CONFIG_DEBUG_DUMP
    if (tx_mic_data_flag)
    {
        //TX_MIC_DATA_DUMP_DATA(data, len);
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
    int rval = 0;
    if (audio_tx_func)
    {
        rval = audio_tx_func(data, len);
    }
    else
    {
        LOGE("send_audio_frame failed, invalid audio_tx_func\n");
        return 0;
    }

    if (rval < 0)
    {
        LOGE("Failed to send audio data, reason: %d\n", rval);
        return 0;
    }
    else
    {
        LOGD("record ret: %d type:%d\n", rval, info.data_type);
    }

    return len;
}



static bk_err_t send_audio_msg(uint16_t len)
{
    bk_err_t ret;
    aud_tras_msg_t msg;

    msg.op = AUD_TRAS_TX_DATA;
    msg.len = len;

    //LOGE("audio send msg: len:%d\n", len);
    if (aud_msg_que)
    {
        ret = rtos_push_to_queue(&aud_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("audio send msg: AUD_TRAS_TX_DATA fail\n");
            return kOverrunErr;
        }

        return ret;
    }
    else
    {
        LOGE("send_audio_msg:aud_msg_que is NULL\n");
    }
    return kNoResourcesErr;
}

static void audio_tras_main(void)
{
    bk_err_t ret = BK_OK;
    GLOBAL_INT_DECLARATION();
    uint32_t size = 0;
    uint32_t count = 0;

    uint8_t *mic_temp_buff = NULL;

    #if 0
    if (agoora_tx_mic_data_flag)
    {
        TX_MIC_DATA_DUMP_OPEN();
    }
    #endif

    rtos_set_semaphore(&aud_sem);

    mic_temp_buff = psram_malloc(SEND_FRAME_SIZE);
    if (NULL == mic_temp_buff)
    {
        LOGE("mic_temp_buff malloc fail\n");
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
                    size = ring_buffer_get_fill_size(&mic_data_rb);
                    //LOGI("rb get, size:%d, msg len:%d\n",size, msg.len);
                    if (size >= msg.len)
                    {
                        GLOBAL_INT_DISABLE();
                        count = ring_buffer_read(&mic_data_rb, mic_temp_buff, msg.len);
                        GLOBAL_INT_RESTORE();
                        //LOGI("msg len:%d, count size:%d\n",msg.len, count);
                        if (count == msg.len)
                        {
                            //LOGE("saf in!\n");   
                            send_audio_frame(mic_temp_buff, count);
                            //LOGE("saf out\n", count,msg.len);
                        }
                        else
                        {
                            LOGE("mic_data_rb count(%d) != frm size:%d\n", count,msg.len);
                        }
                    }
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

    #if 0
    if (tx_mic_data_flag)
    {
        TX_MIC_DATA_DUMP_CLOSE();
    }
    #endif

    if (mic_data_buffer)
    {
        ring_buffer_clear(&mic_data_rb);
        psram_free(mic_data_buffer);
        mic_data_buffer = NULL;
    }

    /* delete msg queue */
    ret = rtos_deinit_queue(&aud_msg_que);
    if (ret != kNoErr)
    {
        LOGE("delete message queue fail\n");
    }
    aud_msg_que = NULL;

    /* delete task */
    aud_thread_hdl = NULL;

    LOGI("delete audio transfer task\n");

    rtos_set_semaphore(&aud_sem);

    rtos_delete_thread(NULL);
}

static int audio_tras_mic_data_handler(uint8_t *data, unsigned int len)
{
    //LOGI("send audio, len:%d\n", len);
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

    return len;
}

static bk_err_t audio_tras_init(void)
{
    bk_err_t ret = BK_OK;

    mic_data_buffer = psram_malloc(MIC_FRAME_SIZE * MIC_FRAME_NUM);
    if (mic_data_buffer == NULL)
    {
        LOGE("malloc mic_data_buffer fail\n");
        return BK_FAIL;
    }
    ring_buffer_init(&mic_data_rb, mic_data_buffer, MIC_FRAME_SIZE * MIC_FRAME_NUM, DMA_ID_MAX, RB_DMA_TYPE_NULL);

    ret = rtos_init_semaphore(&aud_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_init_queue(&aud_msg_que,
                          "tras_queue",
                          sizeof(aud_tras_msg_t),
                          20);
    if (ret != kNoErr)
    {
        LOGE("create agoar audio tras message queue fail\n");
        goto fail;
    }
    LOGI("create audio tras message queue complete\n");

    /* create task to asr */
    ret = rtos_create_thread(&aud_thread_hdl,
                             4,
                             "audio_tras",
                             (beken_thread_function_t)audio_tras_main,
                             2048,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("create audio transfer driver task fail\n");
        aud_thread_hdl = NULL;
        goto fail;
    }

    rtos_get_semaphore(&aud_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("create audio transfer driver task complete\n");

    return BK_OK;

fail:
    if (mic_data_buffer)
    {
        ring_buffer_clear(&mic_data_rb);
        psram_free(mic_data_buffer);
        mic_data_buffer = NULL;
    }

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

    return BK_FAIL;
}

static bk_err_t audio_tras_deinit(void)
{
    bk_err_t ret;
    aud_tras_msg_t msg;

    msg.op = AUD_TRAS_EXIT;
    if (aud_msg_que)
    {
        ret = rtos_push_to_queue_front(&aud_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("audio send msg: AUD_TRAS_EXIT fail\n");
            return BK_FAIL;
        }

        rtos_get_semaphore(&aud_sem, BEKEN_NEVER_TIMEOUT);

        rtos_deinit_semaphore(&aud_sem);
        aud_sem = NULL;
    }

    return BK_OK;
}


int audio_tras_user_audio_rx_data_handle(unsigned char *data, unsigned int size)
{
    bk_err_t ret = BK_OK;

    //LOGE("user_audio_rx size:%d\r\n",size);
    #if CONFIG_DEBUG_DUMP
    if(rx_spk_data_flag)
    {
        //RX_SPK_DATA_DUMP_DATA(data, size);
        #if 0
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_NUM(DUMP_TYPE_RX_SPK,1);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW(DUMP_TYPE_RX_SPK,0,DUMP_FILE_TYPE_G722,size);
        #else
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_RX_SPK,0,size);
        #endif
        DEBUG_DATA_DUMP_UPDATE_HEADER_TIMESTAMP(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_BY_UART_HEADER(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_UPDATE_HEADER_SEQ_NUM(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_BY_UART_DATA(data, size);
    }
    #endif//CONFIG_DEBUG_DUMP

    ret = bk_aud_intf_write_spk_data((uint8_t *)data, (uint32_t)size);
    if (ret != BK_OK)
    {
        LOGE("write spk data fail \r\n");
    }

    return ret;
}

void audio_tras_register_tx_data_func(user_audio_tx_data_func func)
{
    audio_tx_func = func;
}
bk_err_t audio_turn_off(void)
{
    bk_err_t ret =  BK_OK;
    LOGI("%s\n", __func__);


    /* stop voice */
    ret = bk_aud_intf_voc_stop();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, voice stop fail, ret:%d\n", __func__, __LINE__, ret);
    }

    /* deinit vioce */
    ret = bk_aud_intf_voc_deinit();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, voice deinit fail, ret:%d\n", __func__, __LINE__, ret);
    }

    bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_NULL);

    ret = bk_aud_intf_drv_deinit();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, aud_intf driver deinit fail, ret:%d\n", ret);
    }

    audio_tras_deinit();

    RX_SPK_DATA_DUMP_CLOSE();

    return BK_OK;
}

bk_err_t audio_turn_on(void)
{
    bk_err_t ret =  BK_OK;
    LOGI("%s\n", __func__);

    RX_SPK_DATA_DUMP_OPEN();

    aud_intf_drv_setup_t aud_intf_drv_setup = DEFAULT_AUD_INTF_DRV_SETUP_CONFIG();
    aud_intf_voc_setup_t aud_intf_voc_setup = DEFAULT_AUD_INTF_VOC_SETUP_CONFIG();

#ifdef CONFIG_AUD_INTF_SUPPORT_G722
    #if (CONFIG_G722_CODEC_RUN_ON_CPU1)
    aud_intf_voc_setup.data_type  = AUD_INTF_VOC_DATA_TYPE_G722;
    aud_intf_voc_setup.aud_codec_setup_input.enc_frame_len_in_ms = CONFIG_AUDIO_FRAME_DURATION_MS;
    aud_intf_voc_setup.aud_codec_setup_input.dec_frame_len_in_ms = CONFIG_AUDIO_FRAME_DURATION_MS;
    #endif

    #if (CONFIG_G722_CODEC_RUN_ON_CPU0)
    aud_intf_voc_setup.data_type  = AUD_INTF_VOC_DATA_TYPE_PCM;
    aud_intf_voc_setup.aud_codec_setup_input.enc_frame_len_in_ms = CONFIG_AUDIO_FRAME_DURATION_MS;
    aud_intf_voc_setup.aud_codec_setup_input.dec_frame_len_in_ms = CONFIG_AUDIO_FRAME_DURATION_MS;
    #endif    
#elif CONFIG_AUD_INTF_SUPPORT_OPUS
    aud_intf_voc_setup.data_type  = AUD_INTF_VOC_DATA_TYPE_OPUS;
    aud_intf_voc_setup.aud_codec_setup_input.enc_frame_len_in_ms = CONFIG_AUDIO_FRAME_DURATION_MS;
    aud_intf_voc_setup.aud_codec_setup_input.dec_frame_len_in_ms = CONFIG_AUDIO_FRAME_DURATION_MS;
    aud_intf_voc_setup.aud_codec_setup_input.dac_samp_rate = 16000;
#else
    aud_intf_voc_setup.data_type  = AUD_INTF_VOC_DATA_TYPE_G711A;
#endif
    aud_intf_voc_setup.spk_mode   = AUD_DAC_WORK_MODE_DIFFEN;
    aud_intf_voc_setup.aec_enable = AEC_ENABLE;
    aud_intf_voc_setup.samp_rate  = AUDIO_SAMP_RATE;
#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
    aud_intf_voc_setup.mic_gain   = 0x30;
#else
    aud_intf_voc_setup.mic_gain   = 0x3F;
#endif
    aud_intf_voc_setup.spk_gain   = g_volume_gain[volume];
    aud_intf_voc_setup.mic_type = AUD_INTF_MIC_TYPE_BOARD;
    aud_intf_voc_setup.spk_type = AUD_INTF_MIC_TYPE_BOARD;

    bk_aud_intf_aud_codec_init(&aud_intf_voc_setup.aud_codec_setup_input);

    audio_tras_init();

    aud_intf_drv_setup.aud_intf_tx_mic_data = audio_tras_mic_data_handler;

    ret = bk_aud_intf_drv_init(&aud_intf_drv_setup);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, aud_intf driver init fail, ret:%d\n", __func__, __LINE__, ret);
    }

    ret = bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_VOICE);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("%s, %d, aud_intf set_mode fail, ret:%d\n", __func__, __LINE__, ret);
    }


    bk_aud_intf_audio_para_set((app_aud_para_t *)&app_aud_cust_para);
    ret = bk_aud_intf_voc_init(aud_intf_voc_setup);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("bk_aud_intf_voc_init fail, ret:%d \r\n", ret);
    }

    ret = bk_aud_intf_voc_start();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        LOGE("bk_aud_intf_voc_start fail, ret:%d \r\n", ret);
    }

    return BK_OK;
}

