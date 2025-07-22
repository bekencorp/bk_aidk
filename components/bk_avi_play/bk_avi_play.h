#ifndef BK_AVI_PLAY_H
#define BK_AVI_PLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "modules/avilib.h"

#define AVI_VIDEO_USE_HW_DECODE    1

#if CONFIG_PSRAM
#define AVI_VIDEO_MAX_FRAME_LEN    (30 * 1024)
#else
#define AVI_VIDEO_MAX_FRAME_LEN    (15 * 1024)
#endif

typedef struct
{
    avi_t *avi;
    uint32_t *video_frame;
    uint32_t video_len;
    uint32_t video_num;

    uint16_t *framebuffer;
    uint16_t *segmentbuffer;

    uint32_t frame_size;
    uint32_t pos;
    bool segment_flag;
    bool swap_flag;
} bk_avi_play_t;


bk_err_t bk_avi_play_open(const char *filename, bool segment_flag, bool color_swap);

void bk_avi_play_close(void);

void bk_avi_play_start(void);

void bk_avi_play_stop(void);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*BK_AVI_PLAY_H*/

