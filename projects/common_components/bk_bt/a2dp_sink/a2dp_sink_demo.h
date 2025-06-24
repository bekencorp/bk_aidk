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
    A2DP_PLAY_VOTE_FLAG_FROM_A2DP = A2DP_PLAY_VOTE_FLAG_START,
    //A2DP_PLAY_VOTE_FLAG_FROM_AVRCP,
    A2DP_PLAY_VOTE_FLAG_END,
};

int a2dp_sink_demo_init(uint8_t aac_supported);
void a2dp_sink_demo_set_path(uint32_t path);
int32_t bk_bt_app_avrcp_ct_get_attr(uint32_t attr);
bk_err_t a2dp_sink_demo_vote_enable_leagcy(uint8_t enable);
void a2dp_sink_demo_set_mix(uint8_t enable);

void bk_bt_app_avrcp_ct_play(void);
void bk_bt_app_avrcp_ct_pause(void);
void bk_bt_app_avrcp_ct_next(void);
void bk_bt_app_avrcp_ct_prev(void);
void bk_bt_app_avrcp_ct_rewind(uint32_t ms);
void bk_bt_app_avrcp_ct_fast_forward(uint32_t ms);
void bk_bt_app_avrcp_ct_vol_up(void);
void bk_bt_app_avrcp_ct_vol_down(void);
void bk_bt_app_avrcp_ct_vol_change(uint32_t platform_vol);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*A2DP_SINK_DEMO_H*/
