#ifndef __VIDEO_ENGINE_H__
#define __VIDEO_ENGINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "video_config.h"
#include "video_dump_data.h"
#include <driver/media_types.h>
#include <driver/lcd.h>
#include "media_app.h"

typedef void (*user_video_tx_data_func)(frame_buffer_t *frame);
/* API */
bk_err_t voide_engine_init(void);
#if CONFIG_SYS_CPU0
void video_register_tx_data_func(user_video_tx_data_func func);
bk_err_t video_turn_off(void);
bk_err_t video_turn_on(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __VIDEO_ENGINE_H__ */
