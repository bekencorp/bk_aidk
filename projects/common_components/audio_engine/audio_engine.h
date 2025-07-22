#ifndef __AUDIO_ENGINE_H__
#define __AUDIO_ENGINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "audio_config.h"
#include "audio_transfer.h"
#include "audio_config.h"
#include "audio_dump_data.h"
#include <modules/audio_process.h>

typedef struct {
    char encoding_type[20];
    char decoding_type[20];
    uint32_t adc_samp_rate;
    uint32_t dac_samp_rate;
    uint32_t enc_samp_interval;
    uint32_t dec_samp_interval;
    uint32_t enc_node_size;
    uint32_t dec_node_size;
} audio_info_t;

extern audio_info_t general_audio;
typedef int (*user_audio_end_func)(void *param);

/* API */
bk_err_t audio_engine_init(void);
bk_err_t audio_turn_on(void);
bk_err_t audio_turn_off(void);
void audio_register_play_finish_func(user_audio_end_func func);
void audio_register_tone_play_finish_func(user_audio_end_func func);

#ifdef __cplusplus
}
#endif
#endif /* __AUDIO_ENGINE_H__ */
