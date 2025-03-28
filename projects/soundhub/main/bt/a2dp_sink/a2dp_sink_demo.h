/**
 * @file a2dp_sink_demo.h
 *
 */

#ifndef A2DP_SINK_DEMO_H
#define A2DP_SINK_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum
{
    A2DP_PLAY_VOTE_FLAG_START,
    A2DP_PLAY_VOTE_FLAG_FROM_BT = A2DP_PLAY_VOTE_FLAG_START,
    A2DP_PLAY_VOTE_FLAG_FROM_AI_VOICE,
    A2DP_PLAY_VOTE_FLAG_END,
};

int a2dp_sink_demo_init(uint8_t aac_supported);
void a2dp_sink_demo_set_path(uint32_t path);
int32_t bk_bt_app_avrcp_ct_get_attr(uint32_t attr);
void a2dp_sink_demo_vote_enable(uint8_t enable, uint32_t flag);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*A2DP_SINK_DEMO_H*/
