#include "media_main.h"
#include "media_audio.h"
#include "audio_play.h"
#include "resource/resource.h"

#include <driver/gpio.h>
#include "gpio_driver.h"

#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

enum
{
    AUDIO_PLAY_STATUS_IDLE,
    AUDIO_PLAY_STATUS_PLAY,
};

typedef struct
{
    const uint8_t *data_p;
    const unsigned int *data_len;
} audio_tone_map_t;

#define CONSTANT_PLAY 0

static audio_play_t *s_audio_play_obj;
static beken_semaphore_t s_audio_ctrl_sem = NULL;
static const audio_tone_map_t s_audio_tone_map[TONE_ENUM_END] =
{
    [TONE_ENUM_CANT_DET] = {cant_det_pcm, &cant_det_pcm_len},
    [TONE_ENUM_DETECTING] = {detecting_pcm, &detecting_pcm_len},
    [TONE_ENUM_DRAW] = {draw_pcm, &draw_pcm_len},
    [TONE_ENUM_GET_READY] = {get_ready_pcm, &get_ready_pcm_len},
    [TONE_ENUM_MY_SHOW] = {my_show_pcm, &my_show_pcm_len},
    [TONE_ENUM_PAPER] = {paper_pcm, &paper_pcm_len},
    [TONE_ENUM_PLS_STOP_HAND] = {pls_stop_hand_pcm, &pls_stop_hand_pcm_len},
    [TONE_ENUM_ROCK] = {rock_pcm, &rock_pcm_len},
    [TONE_ENUM_SCISSORS] = {scissors_pcm, &scissors_pcm_len},
    [TONE_ENUM_YOU_LOSS] = {you_loss_pcm, &you_loss_pcm_len},
    [TONE_ENUM_YOU_WIN] = {you_win_pcm, &you_win_pcm_len},
    [TONE_ENUM_YOUR_SHOW] = {your_show_pcm, &your_show_pcm_len},
};

static uint8_t s_need_sync;
static uint8_t s_audio_play_status = AUDIO_PLAY_STATUS_IDLE;

static int empty_notify(void *play_ctx, void *params)
{
    uint8_t *need_sync = (typeof(need_sync))params;
    appm_logi("%d status %d", *need_sync, s_audio_play_status);

    if (s_audio_play_status != AUDIO_PLAY_STATUS_IDLE)
    {
        s_audio_play_status = AUDIO_PLAY_STATUS_IDLE;

        if (s_audio_ctrl_sem && *need_sync)
        {
            rtos_set_semaphore(&s_audio_ctrl_sem);
        }
    }

    return 0;
}

int32_t audio_play(uint32_t tone_type, uint8_t sync)
{
    bk_err_t ret = 0;

    if (!s_audio_play_obj || !s_audio_ctrl_sem)
    {
        appm_loge("not init");
        ret = -1;
        goto end;
    }

    appm_logi("type %d %d", tone_type, sync);

    s_need_sync = sync;
    s_audio_play_status = AUDIO_PLAY_STATUS_PLAY;
#if CONSTANT_PLAY

    if ((ret = audio_play_control(s_audio_play_obj, AUDIO_PLAY_RESUME)) != 0)
    {
        appm_loge("AUDIO_PLAY_RESUME err", ret);
        goto end;
    }

#else

    if ((ret = audio_play_open(s_audio_play_obj)) != 0)
    {
        appm_loge("open audio play err", ret);

        goto end;
    }

#endif
    ret = audio_play_write_data(s_audio_play_obj, (char *)s_audio_tone_map[tone_type].data_p, *s_audio_tone_map[tone_type].data_len);

    if (ret != *s_audio_tone_map[tone_type].data_len)
    {
        appm_loge("write err %d %d", ret, *s_audio_tone_map[tone_type].data_len);
    }

    appm_logi("write compl %d", *s_audio_tone_map[tone_type].data_len);

    if (sync)
    {
        rtos_get_semaphore(&s_audio_ctrl_sem, BEKEN_WAIT_FOREVER);
#if CONSTANT_PLAY

        if ((ret = audio_play_control(s_audio_play_obj, AUDIO_PLAY_PAUSE)) != 0)
        {
            appm_loge("AUDIO_PLAY_PAUSE err", ret);
            goto end;
        }

#else
        audio_play_close(s_audio_play_obj);
#endif
    }

    return ret;
end:;

    if (s_audio_play_obj)
    {
#if CONSTANT_PLAY
#else
        audio_play_close(s_audio_play_obj);
#endif
    }

    return ret;
}

int32_t audio_wait_play_end(void * (*cb)(void), void (*free_cb)(void *arg), uint32_t ms)
{
    int32_t ret = 0;
    void *arg = NULL;

    while (s_audio_play_status != AUDIO_PLAY_STATUS_IDLE)
    {
        if (arg)
        {
            free_cb(arg);
        }

        arg = cb();

        rtos_delay_milliseconds(ms);
    }

#if CONSTANT_PLAY

    if ((ret = audio_play_control(s_audio_play_obj, AUDIO_PLAY_PAUSE)) != 0)
    {
        appm_loge("AUDIO_PLAY_PAUSE err", ret);
    }

#else
    audio_play_close(s_audio_play_obj);
#endif

    return 0;
}

int32_t audio_init(void)
{
    int32_t ret = 0 ;
    audio_play_cfg_t cfg = DEFAULT_AUDIO_PLAY_CONFIG();

    cfg.nChans = 1;
    cfg.sampRate = 8000;
    cfg.volume = 0x1d;
    cfg.frame_size = cfg.sampRate *cfg.nChans / 1000 * cfg.bitsPerSample / 8 * 10; //10 sec
    cfg.pool_size = cfg.frame_size * 3;
    cfg.pool_empty_notify_cb = empty_notify;
    cfg.usr_data = &s_need_sync;
    cfg.pa_ctl_en = true;
    cfg.pa_ctl_gpio = 50;
    cfg.pa_on_level = 1;
    cfg.pa_on_delay = 2;
    cfg.pa_off_delay = 2;

    if (!s_audio_ctrl_sem)
    {
        if (kNoErr != rtos_init_semaphore(&s_audio_ctrl_sem, 1))
        {
            appm_loge("init sema fail");
            goto end;
        }
    }

    s_audio_play_obj = audio_play_create(AUDIO_PLAY_ONBOARD_SPEAKER, &cfg);

    if (!s_audio_play_obj)
    {
        appm_loge("create audio play err");

        goto end;
    }

    gpio_dev_unmap(GPIO_50);
    bk_gpio_set_capacity(GPIO_50, 0);
    BK_LOG_ON_ERR(bk_gpio_disable_input(GPIO_50));
    BK_LOG_ON_ERR(bk_gpio_enable_output(GPIO_50));
    BK_LOG_ON_ERR(bk_gpio_set_output_high(GPIO_50));

#if CONSTANT_PLAY

    if ((ret = audio_play_open(s_audio_play_obj)) != 0)
    {
        appm_loge("open audio play err", ret);

        goto end;
    }

    if ((ret = audio_play_control(s_audio_play_obj, AUDIO_PLAY_PAUSE)) != 0)
    {
        appm_loge("AUDIO_PLAY_PAUSE err", ret);
        goto end;
    }

#endif
end:;

    return ret;
}


int32_t audio_deinit(void)
{
    int32_t ret = 0 ;

    if (s_audio_play_obj)
    {
        audio_play_close(s_audio_play_obj);
        audio_play_destroy(s_audio_play_obj);
        s_audio_play_obj = NULL;
    }

end:;

    return ret;
}
