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

int a2dp_sink_demo_init(uint8_t aac_supported);
void a2dp_sink_demo_set_path(uint32_t path);
int32_t bk_bt_app_avrcp_ct_get_attr(uint32_t attr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*A2DP_SINK_DEMO_H*/
