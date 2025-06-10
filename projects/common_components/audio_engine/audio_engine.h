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


/* API */
bk_err_t audio_engine_init(void);
bk_err_t audio_turn_on(void);
bk_err_t audio_turn_off(void);

#ifdef __cplusplus
}
#endif
#endif /* __AUDIO_ENGINE_H__ */
