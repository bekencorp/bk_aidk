#include <common/bk_include.h>
#include <modules/pm.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#include "audio_play.h"
#include "play_voice.h"
#include <driver/audio_ring_buff.h>
#include <modules/g722.h>

#define TAG "voc_ply"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define VOICE_FRAME_SIZE            (160)   //16K G722 20ms
#define VOICE_FRAME_PCM_SIZE        (VOICE_FRAME_SIZE * 4)   //16K pcm 20ms


#define TEST_CHECK_NULL(ptr) do {\
        if (ptr == NULL) {\
            BK_LOGI(TAG, "TEST_CHECK_NULL fail \n");\
            return BK_FAIL;\
        }\
    } while(0)

static beken_thread_t play_voice_task_hdl = NULL;
static beken_queue_t play_voice_msg_que = NULL;
static beken_semaphore_t play_voice_sem = NULL;
static RingBufferContext *spk_data_rb = NULL;

static audio_play_t *gl_audio_play = NULL;
static g722_decode_state_t *gl_g722_dec = NULL;


bk_err_t play_voice_send_msg(play_voice_op_t op, void *param)
{
    bk_err_t ret;
    play_voice_msg_t msg;

    msg.op = op;
    msg.param = param;
    if (play_voice_msg_que)
    {
        ret = rtos_push_to_queue(&play_voice_msg_que, &msg, BEKEN_NO_WAIT);
        if (kNoErr != ret)
        {
            LOGE("play_voice_send_msg fail\n");
            return kOverrunErr;
        }

        return ret;
    }
    return kNoResourcesErr;
}


static void play_voice_task_main(beken_thread_arg_t param_data)
{
    bk_err_t ret = BK_OK;
    int read_size = 0;
    static bool play_voice_flag = false;

    uint8_t *g722_temp_buff = NULL;
    int16_t *spk_temp_buff = NULL;
    g722_temp_buff = psram_malloc(VOICE_FRAME_SIZE);
    if (g722_temp_buff == NULL)
    {
        LOGE("psram_malloc g722_temp_buff: %d fail\n", VOICE_FRAME_SIZE);
    }
    os_memset(g722_temp_buff, 0, VOICE_FRAME_SIZE);

    spk_temp_buff = psram_malloc(VOICE_FRAME_PCM_SIZE);
    if (spk_temp_buff == NULL)
    {
        LOGE("psram_malloc spk_temp_buff: %d fail\n", VOICE_FRAME_PCM_SIZE);
    }
    os_memset(spk_temp_buff, 0, VOICE_FRAME_PCM_SIZE);

    rtos_set_semaphore(&play_voice_sem);

    play_voice_msg_t msg;
    while (1)
    {
        ret = rtos_pop_from_queue(&play_voice_msg_que, &msg, 0);//BEKEN_WAIT_FOREVER
        if (kNoErr == ret)
        {
            switch (msg.op)
            {
                case PLAY_VOICE_IDLE:
                    LOGI("Stop play voice of AI Agent\n");
                    play_voice_flag = false;
                    break;

                case PLAY_VOICE_EXIT:
                    LOGD("goto: PLAY_VOICE_EXIT\n");
                    goto exit;
                    break;

                case PLAY_VOICE_PLAY:
                    LOGI("start play voice of AI Agent\n");
                    play_voice_flag = true;
                    break;

                default:
                    break;
            }
        }

        /* read aud_rx_rb data received from AI Agent */
        uint32_t free_size = ring_buffer_get_fill_size(spk_data_rb);
        if (free_size >= VOICE_FRAME_SIZE)
        {
            read_size = ring_buffer_read(spk_data_rb, g722_temp_buff, VOICE_FRAME_SIZE);
            if (read_size == VOICE_FRAME_SIZE)
            {
                /* check whether play voice data */
                if (play_voice_flag)
                {
                    /* decode g722 data to pcm */
                    int dec_size = g722_decode(gl_g722_dec, spk_temp_buff, g722_temp_buff, read_size);
                    if (dec_size != VOICE_FRAME_PCM_SIZE / 2)
                    {
                        os_printf("g722_decode fail, dec_size: %d != %d\n", dec_size, VOICE_FRAME_PCM_SIZE / 2);
                        break;
                    }

                    uint32_t write_ret = audio_play_write_data(gl_audio_play, (char *)spk_temp_buff, VOICE_FRAME_PCM_SIZE);
                    if (write_ret != VOICE_FRAME_PCM_SIZE)
                    {
                        LOGE("play_pipeline_write_spk_data fail, ret: %d != %d\n", write_ret, VOICE_FRAME_PCM_SIZE);
                    }
                    else
                    {
                        LOGD("voice play, ret: %d\n", write_ret);
                    }
                }
            }
            else
            {
                LOGE("ring_buffer_read fail: %d != %d\n", read_size, VOICE_FRAME_SIZE);
            }
        }
        else
        {
            rtos_delay_milliseconds(5);
        }
    }

exit:
    /* delete msg queue */
    ret = rtos_deinit_queue(&play_voice_msg_que);
    if (ret != kNoErr)
    {
        LOGE("delete play_voice message queue fail\n");
    }
    play_voice_msg_que = NULL;
    LOGI("delete play_voice task\n");

    /* delete task */
    play_voice_task_hdl = NULL;

    rtos_set_semaphore(&play_voice_sem);

    rtos_delete_thread(NULL);
}

static bk_err_t play_voice_main_task_init(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&play_voice_sem, 1);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, create semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_init_queue(&play_voice_msg_que,
                          "play_voice_que",
                          sizeof(play_voice_msg_t),
                          2);
    if (ret != kNoErr)
    {
        LOGE("create play_voice message queue fail\n");
        goto fail;
    }
    LOGI("create play_voice message queue complete\n");

    ret = rtos_create_thread(&play_voice_task_hdl,
                             6,
                             "play_voice",
                             (beken_thread_function_t)play_voice_task_main,
                             1024,
                             NULL);
    if (ret != kNoErr)
    {
        LOGE("Error: Failed to create play_voice task \n");
        play_voice_msg_que = NULL;
        goto fail;
    }

    rtos_get_semaphore(&play_voice_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("init play_voice task complete \n");

    return BK_OK;

fail:

    if (play_voice_sem)
    {
        rtos_deinit_semaphore(&play_voice_sem);
        play_voice_sem = NULL;
    }

    if (play_voice_msg_que)
    {
        rtos_deinit_queue(&play_voice_msg_que);
        play_voice_msg_que = NULL;
    }

    return BK_FAIL;
}

bk_err_t play_voice_init(RingBufferContext *spk_rb, audio_play_t *audio_play)
{
    bk_err_t ret = BK_OK;

    if (play_voice_task_hdl)
    {
        LOGE("%s, %d, play_voice already init\n", __func__, __LINE__);
        play_voice_deinit();
    }

    spk_data_rb = spk_rb;
    gl_audio_play = audio_play;

    gl_g722_dec = (g722_decode_state_t *)psram_malloc(sizeof(g722_decode_state_t));
    if (!gl_g722_dec)
    {
        LOGE("%s, %d, malloc gl_g722_dec\n", __func__, __LINE__);
        goto fail;
    }
    os_memset(gl_g722_dec, 0, sizeof(g722_decode_state_t));

    if (0 != g722_decode_init(gl_g722_dec, 64000, 0))
    {
        LOGE("g722_decode_init fail.\n");
        goto fail;
    }

    /* init audio control main task */
    ret = play_voice_main_task_init();
    if (ret != BK_OK)
    {
        spk_data_rb = NULL;
        gl_audio_play = NULL;
        goto fail;
    }

    return BK_OK;

fail:
    if (gl_g722_dec)
    {
        g722_decode_release(gl_g722_dec);
        psram_free(gl_g722_dec);
        gl_g722_dec = NULL;
    }

    return BK_FAIL;
}

bk_err_t play_voice_deinit(void)
{
    if (!play_voice_task_hdl)
    {
        LOGE("%s, %d, play_voice already deinit\n", __func__, __LINE__);
        return BK_OK;
    }

    play_voice_stop();

    play_voice_send_msg(PLAY_VOICE_EXIT, NULL);

    rtos_get_semaphore(&play_voice_sem, BEKEN_NEVER_TIMEOUT);

    if (play_voice_sem)
    {
        rtos_deinit_semaphore(&play_voice_sem);
        play_voice_sem = NULL;
    }

    if (gl_g722_dec)
    {
        g722_decode_release(gl_g722_dec);
        psram_free(gl_g722_dec);
        gl_g722_dec = NULL;
    }

    return BK_OK;
}

bk_err_t play_voice_start(void)
{
    play_voice_send_msg(PLAY_VOICE_PLAY, NULL);

    return BK_OK;
}

bk_err_t play_voice_stop(void)
{
    play_voice_send_msg(PLAY_VOICE_IDLE, NULL);

    /* wait write speaker data complete and set state to idle */
    //TODO


    return BK_OK;
}


