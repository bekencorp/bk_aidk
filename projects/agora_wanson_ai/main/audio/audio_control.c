#include <common/bk_include.h>
#include <modules/pm.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#include "voice_asr.h"
#include "audio_record.h"

#include "audio_play.h"
#include "audio_control.h"
//#include "bell_8k_16bit_mono_pcm.h"
#include "bell_16k_16bit_mono_pcm.h"
#include "modules/g722.h"

#include "media_evt.h"
#include "media_mailbox_list_util.h"
#include "play_voice.h"


#define TAG "aud_ctl"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define RECORD_READ_FRAME_SIZE      (640)   //16k pcm 20ms
#define RECORD_G722_FRAME_SIZE      (RECORD_READ_FRAME_SIZE/4)   //16k g722 20ms

#define RECORD_MIC_TIME             (5000)  //ms

#define BELL_FRAME_SIZE             (640)   //16K 16bit mono 20ms


static audio_mode_t audio_mode = AUDIO_MODE_IDLE;
static beken_thread_t audio_control_task_hdl = NULL;
static beken_queue_t audio_control_msg_que = NULL;
static beken_semaphore_t audio_control_sem = NULL;
static beken_timer_t record_timer = {0};
static bool record_work_flag = false;
static uint8_t *aud_mic_data  = NULL;
//#define ASR_BUFF_SIZE 8000  //>960*2

static RingBufferContext *aud_tx_rb = NULL;
static RingBufferContext *aud_rx_rb = NULL;
static media_mailbox_msg_t mic_to_media_app_msg = {0};

static audio_play_t *gl_audio_play = NULL;
static audio_record_t *gl_audio_record = NULL;
static g722_encode_state_t *gl_g722_enc = NULL;

static beken_semaphore_t gl_play_bell_sem = NULL;
static beken_semaphore_t gl_record_question_sem = NULL;
static bool gl_abort_play_bell_flag = true;
static bool gl_abort_record_question_flag = false;

static bk_err_t audio_control_set_mode(audio_mode_t mode)
{
    audio_mode = mode;

    return BK_OK;
}

bk_err_t audio_control_send_msg(audio_control_op_t op, void *param)
{
    bk_err_t ret;
    audio_control_msg_t msg;

    msg.op = op;
    msg.param = param;
    if (audio_control_msg_que)
    {
        ret = rtos_push_to_queue(&audio_control_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("send_mic_data_send_msg fail \r\n");
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}

static bk_err_t play_bell(char *bell_data, uint32_t size)
{
    uint32_t total_size = size;
    uint32_t read_total_size = 0;
    bk_err_t write_ret = 0;

    LOGI("play bell start\n");

    gl_abort_play_bell_flag = false;

    while (!gl_abort_play_bell_flag && total_size >= BELL_FRAME_SIZE)
    {
        write_ret = audio_play_write_data(gl_audio_play, &bell_data[read_total_size], BELL_FRAME_SIZE);
        if (write_ret != BELL_FRAME_SIZE)
        {
            LOGE("play_pipeline_write_spk_data fail, ret: %d\n", write_ret);
            return BK_FAIL;
        }

        read_total_size += BELL_FRAME_SIZE;
        total_size -= BELL_FRAME_SIZE;
    }

    if (gl_abort_play_bell_flag)
    {
        rtos_set_semaphore(&gl_play_bell_sem);
    }

    gl_abort_play_bell_flag = true;

    LOGI("play bell complete\n");

    return BK_OK;
}

static bk_err_t abort_play_bell(void)
{
    if (!gl_abort_play_bell_flag)
    {
        gl_abort_play_bell_flag = true;

        rtos_get_semaphore(&gl_play_bell_sem, BEKEN_NEVER_TIMEOUT);
    }

    return BK_OK;
}

/* record and send mic data encoded by g711a to AI Agent */
static bk_err_t record_mode_handle(void)
{
    bk_err_t ret = BK_OK;

    uint8_t *g722_enc_buf = NULL;
    g722_enc_buf = psram_malloc(RECORD_G722_FRAME_SIZE);
    if (g722_enc_buf == NULL)
    {
        LOGE("psram_malloc g722_enc_buf: %d fail\n", RECORD_G722_FRAME_SIZE);
    }
    os_memset(g722_enc_buf, 0, RECORD_G722_FRAME_SIZE);

    ret = rtos_start_timer(&record_timer);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, start record timer fail, cannot listening\n", __func__, __LINE__);
        goto exit;
    }
    LOGI("start record, please ask questions \n");
    LOGI("listening...... \n");

    record_work_flag = true;

    while (record_work_flag && !gl_abort_record_question_flag)
    {
        ret = audio_record_read_data(gl_audio_record, (char *)aud_mic_data, RECORD_READ_FRAME_SIZE);
        if (ret != RECORD_READ_FRAME_SIZE)
        {
            LOGE("%s, %d, read mic data: %d != %d \n", __func__, __LINE__, ret, RECORD_READ_FRAME_SIZE);
            rtos_stop_timer(&record_timer);
            record_work_flag = false;
            gl_abort_record_question_flag = true;
            break;
        }

        /* g722 encoder pcm data to g722 */
        int enc_size = g722_encode(gl_g722_enc, g722_enc_buf, (int16_t *)aud_mic_data, RECORD_READ_FRAME_SIZE / 2);
        if (enc_size != RECORD_G722_FRAME_SIZE)
        {
            LOGE("g722_encode fail, enc_size: %d\n", enc_size);
            continue;
        }

        /* send mic data to AI Agent */
        ring_buffer_write(aud_tx_rb, (uint8_t *)g722_enc_buf, RECORD_G722_FRAME_SIZE);

        /* send msg to aud_tras */
        mic_to_media_app_msg.event = EVENT_AUD_MIC_DATA_NOTIFY;
        mic_to_media_app_msg.param = RECORD_G722_FRAME_SIZE;
        msg_send_notify_to_media_major_mailbox(&mic_to_media_app_msg, APP_MODULE);
    }

    LOGI("listening end \n");
    LOGI("record and send questions to AI Agent complate\n", __func__, __LINE__);

exit:
    psram_free(g722_enc_buf);
    g722_enc_buf = NULL;

    if (!gl_abort_record_question_flag)
    {
        /* record mic and send to AI Agent complete */
        audio_control_send_msg(AUDIO_CONTROL_PLAY, NULL);
    }

    if (gl_abort_play_bell_flag)
    {
        rtos_stop_timer(&record_timer);
        record_work_flag = false;
        rtos_set_semaphore(&gl_record_question_sem);
        gl_abort_record_question_flag = false;
    }

    return BK_OK;
}

static bk_err_t abort_record_question(void)
{
    /* Check whether record question working */
    if (record_work_flag)
    {
        gl_abort_record_question_flag = true;

        rtos_get_semaphore(&gl_record_question_sem, BEKEN_NEVER_TIMEOUT);
    }

    return BK_OK;
}

static void audio_control_task_main(beken_thread_arg_t param_data)
{
    bk_err_t ret = BK_OK;

    rtos_set_semaphore(&audio_control_sem);

    audio_control_msg_t msg;
    while (1)
    {
        ret = rtos_pop_from_queue(&audio_control_msg_que, &msg, BEKEN_WAIT_FOREVER);
        if (kNoErr == ret)
        {
            switch (msg.op)
            {
                case AUDIO_CONTROL_IDLE:
                    LOGD("audio_control status: idle\n");
                    audio_control_set_mode(AUDIO_MODE_IDLE);
                    break;

                case AUDIO_CONTROL_EXIT:
                    LOGD("goto: AUDIO_CONTROL_EXIT \r\n");
                    goto audio_control_exit;
                    break;

                case AUDIO_CONTROL_ASR:
                    LOGI("audio_control status: asr\n");
                    audio_control_set_mode(AUDIO_MODE_ASR);
                    break;

                case AUDIO_CONTROL_BELL:
                    LOGD("audio_control status: bell\n");
                    audio_control_set_mode(AUDIO_MODE_BELL);
                    /* stop voice play, start record and send mic data of 8Khz to AI Agent */
                    play_voice_stop();
                    /* play bell */
                    //rtos_delay_milliseconds(500);
                    //play_bell((char *)bell_8k_16bit_mono_pcm, sizeof(bell_8k_16bit_mono_pcm));
                    play_bell((char *)bell_16k_16bit_mono_pcm, sizeof(bell_16k_16bit_mono_pcm));
                    audio_control_send_msg(AUDIO_CONTROL_RECORD, NULL);
                    break;

                case AUDIO_CONTROL_RECORD:
                    LOGD("audio_control status: record\n");
                    audio_control_set_mode(AUDIO_MODE_RECORD);
                    record_mode_handle();
                    break;

                case AUDIO_CONTROL_PLAY:
                    LOGD("audio_control status: play\n");
                    audio_control_set_mode(AUDIO_MODE_PLAY);
                    /* start play voice received from AI Agent */
                    play_voice_start();
                    /* start voice asr to check armino */
                    voice_asr_start();
                    break;

                default:
                    break;
            }
        }
    }

audio_control_exit:
    /* delete msg queue */
    ret = rtos_deinit_queue(&audio_control_msg_que);
    if (ret != kNoErr)
    {
        LOGE("delete audio_control message queue fail\n");
    }
    audio_control_msg_que = NULL;

    /* delete task */
    audio_control_task_hdl = NULL;

    LOGI("delete audio_control task\n");

    rtos_set_semaphore(&audio_control_sem);

    rtos_delete_thread(NULL);
}

static bk_err_t audio_control_main_task_init(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&audio_control_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_init_queue(&audio_control_msg_que,
                          "audio_control_que",
                          sizeof(audio_control_msg_t),
                          5);
    if (ret != kNoErr)
    {
        LOGE("create audio control message queue fail\n");
        goto fail;
    }
    LOGI("create audio control message queue complete\n");

    ret = rtos_create_thread(&audio_control_task_hdl,
                             5,
                             "audio_control",
                             (beken_thread_function_t)audio_control_task_main,
                             1024,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("%s, %d, create audio_control task\n", __func__, __LINE__);
        audio_control_msg_que = NULL;
        goto fail;
    }

    rtos_get_semaphore(&audio_control_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("init audio_control task complete\n");

    return BK_OK;

fail:

    if (audio_control_sem)
    {
        rtos_deinit_semaphore(&audio_control_sem);
        audio_control_sem = NULL;
    }

    if (audio_control_msg_que)
    {
        rtos_deinit_queue(&audio_control_msg_que);
        audio_control_msg_que = NULL;
    }

    return BK_FAIL;
}

static void record_timer_callback(void *param)
{
    bk_err_t ret = BK_OK;

    /* stop record and set work mode to play */
    ret = rtos_stop_timer(&record_timer);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, stop record timer fail \n", __func__, __LINE__);
    }

    record_work_flag = false;
}

bk_err_t audio_control_deinit(void)
{
    /* deinit voice asr task */
    voice_asr_deinit();

    /* deinit play voice task */
    play_voice_deinit();

    /* free audio_play */
    if (gl_audio_play)
    {
        audio_play_destroy(gl_audio_play);
        gl_audio_play = NULL;
    }

    /* free audio_record */
    if (gl_audio_record)
    {
        audio_record_destroy(gl_audio_record);
        gl_audio_record = NULL;
    }

    /* deinit timer */
    if (rtos_is_timer_init(&record_timer))
    {
        rtos_deinit_timer(&record_timer);
    }

    /* deinit audio control task */
    if (audio_control_sem)
    {
        audio_control_send_msg(AUDIO_CONTROL_EXIT, NULL);
        rtos_get_semaphore(&audio_control_sem, BEKEN_NEVER_TIMEOUT);
        rtos_deinit_semaphore(&audio_control_sem);
        audio_control_sem = NULL;
    }

    if (aud_mic_data)
    {
        psram_free(aud_mic_data);
        aud_mic_data = NULL;
    }

    if (gl_g722_enc)
    {
        g722_encode_release(gl_g722_enc);
        psram_free(gl_g722_enc);
        gl_g722_enc = NULL;
    }

    if (gl_play_bell_sem)
    {
        rtos_deinit_semaphore(&gl_play_bell_sem);
        gl_play_bell_sem = NULL;
    }

    if (gl_record_question_sem)
    {
        rtos_deinit_semaphore(&gl_record_question_sem);
        gl_record_question_sem = NULL;
    }

    return BK_OK;
}

bk_err_t audio_control_init(void)
{
    bk_err_t ret = BK_OK;

    /* init timer */
    ret = rtos_init_timer(&record_timer, RECORD_MIC_TIME, record_timer_callback, NULL);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, init %s record timer fail, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }

    aud_mic_data = (uint8_t *)psram_malloc(RECORD_READ_FRAME_SIZE);
    if (!aud_mic_data)
    {
        LOGE("%s, %d, malloc aud_mic_data\n", __func__, __LINE__);
        goto fail;
    }
    os_memset(aud_mic_data, 0, RECORD_READ_FRAME_SIZE);

    ret = rtos_init_semaphore(&gl_play_bell_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create play bell semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    ret = rtos_init_semaphore(&gl_record_question_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create record question semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    /* init audio control main task */
    ret = audio_control_main_task_init();
    if (ret != BK_OK)
    {
        LOGE("%s, %d, init audio control task fail, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }

    /* init audio record */
    audio_record_cfg_t record_config = DEFAULT_AUDIO_RECORD_CONFIG();
    record_config.sampRate = 16000;
    record_config.frame_size = RECORD_READ_FRAME_SIZE;
    record_config.pool_size = RECORD_READ_FRAME_SIZE * 2;
    record_config.adc_gain = 0x3f;
    gl_audio_record = audio_record_create(AUDIO_RECORD_ONBOARD_MIC, &record_config);
    if (!gl_audio_record)
    {
        LOGE("%s, %d, create audio record fail\n", __func__, __LINE__);
        goto fail;
    }

    /* init audio play */
    audio_play_cfg_t play_config = DEFAULT_AUDIO_PLAY_CONFIG();
    play_config.sampRate = 16000;
    //play_config.volume = 0x35;
    play_config.frame_size = 640;
    play_config.pool_size = 1280;
    gl_audio_play = audio_play_create(AUDIO_PLAY_ONBOARD_SPEAKER, &play_config);
    if (!gl_audio_play)
    {
        LOGE("%s, %d, create audio play fail\n", __func__, __LINE__);
        goto fail;
    }

    /* init g722 encoder */
    gl_g722_enc = (g722_encode_state_t *)psram_malloc(sizeof(g722_encode_state_t));
    if (!gl_g722_enc)
    {
        LOGE("%s, %d, malloc gl_g722_enc\n", __func__, __LINE__);
        goto fail;
    }
    os_memset(gl_g722_enc, 0, sizeof(g722_encode_state_t));

    if (0 != g722_encode_init(gl_g722_enc, 64000, 0))
    {
        LOGE("%s, %d, g722_encode_init fail.\n", __func__, __LINE__);
        goto fail;
    }

    /* init play voice task */
    ret = play_voice_init(aud_rx_rb, gl_audio_play);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, init play voice task fail, ret:%d\n", __func__, __LINE__, ret);
        goto fail;
    }

    /* init voice asr */
    if (BK_OK != voice_asr_init(gl_audio_record))
    {
        LOGE("%s, %d, voice_asr_init fail\n", __func__, __LINE__);
        goto fail;
    }

    return BK_OK;

fail:
    audio_control_deinit();

    return BK_FAIL;
}


bk_err_t audio_control_start(void)
{
    bk_err_t ret = BK_OK;

    ret = audio_record_open(gl_audio_record);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, audio record open fail, ret:%d\n", __func__, __LINE__, ret);
        return BK_FAIL;
    }

    ret = voice_asr_start();
    if (ret != BK_OK)
    {
        LOGE("%s, %d, wanson asr start fail, ret:%d\n", __func__, __LINE__, ret);
        return BK_FAIL;
    }

    ret = audio_play_open(gl_audio_play);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, audio play open fail, ret:%d\n", __func__, __LINE__, ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t audio_control_stop(void)
{
    bk_err_t ret = BK_OK;

    /* stop asr */
    voice_asr_stop();
    /* wait 40ms(>20ms) to make sure that wanson_asr read mic data complete and wanson asr task state is idle. */
    rtos_delay_milliseconds(40);

    /* reset queue to avoid handle asr result in queue */
    rtos_reset_queue(&audio_control_msg_que);

    /* stop record and send question to AI Agent */
    abort_record_question();

    play_voice_stop();
    /* wait 40ms(>20ms) to make sure that play voice write speaker data complete and voice play task state is idle. */
    rtos_delay_milliseconds(40);

    ret = audio_record_close(gl_audio_record);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, audio_record_close fail, ret:%d\n", __func__, __LINE__, ret);
    }

    /* stop play bell */
    abort_play_bell();

    ret = audio_play_close(gl_audio_play);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, audio_play_close fail, ret:%d\n", __func__, __LINE__, ret);
    }

    return BK_OK;
}

bk_err_t audio_control_event_handle(media_mailbox_msg_t *msg)
{
    bk_err_t ret = BK_FAIL;

    /* save mailbox msg received from media app */
    LOGI("%s, %d, event: %d \n", __func__, __LINE__, msg->event);

    switch (msg->event)
    {
        case EVENT_AUD_AI_INIT_REQ:
        {
            /* init audio control */
            audio_rb_ctx_t *config = (audio_rb_ctx_t *)msg->param;
            aud_tx_rb = &config->aud_tx_rb;
            aud_rx_rb = &config->aud_rx_rb;
            ret = audio_control_init();
            msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
            break;
        }

        case EVENT_AUD_AI_START_REQ:
        {
            /* start audio control */
            ret = audio_control_start();
            msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
            break;
        }

        case EVENT_AUD_AI_STOP_REQ:
        {
            /* stop audio control */
            ret = audio_control_stop();
            msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
            break;
        }

        case EVENT_AUD_AI_DEINIT_REQ:
        {
            /* deinit audio control */
            ret = audio_control_deinit();
            msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
            break;
        }

        default:
            break;
    }

    return ret;
}

