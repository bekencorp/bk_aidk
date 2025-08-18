// Copyright 2025-2035 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __BK_WSS_H__
#define __BK_WSS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "aud_intf.h"

typedef enum
{
    WSS_EVENT_RECORDING_START           = 0,
    WSS_EVENT_RECORDING_END,
    WSS_EVENT_LLM_COMPLETE,
    WSS_EVENT_PLAYING_START,
    WSS_EVENT_PLAYING_END,
} wss_evt_state_type_t;

void rtc_get_aud_inft_info(aud_intf_voc_setup_t *aud_intf_voc_setup, char *encoder_name, char *decoder_name);
void rtc_get_dialog_info(aud_intf_voc_setup_t *aud_intf_voc_setup, char *encoder_name, char *decoder_name);
void rtc_websocket_rx_data_clean(void);
int32_t bk_wss_state_event(uint32_t event, void *param);



#ifdef __cplusplus
}
#endif
#endif /* __AGORA_RTC_H__ */
