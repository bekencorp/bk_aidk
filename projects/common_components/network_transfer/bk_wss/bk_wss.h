/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#ifndef __BK_WSS_H__
#define __BK_WSS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "aud_intf.h"

void rtc_get_aud_inft_info(aud_intf_voc_setup_t *aud_intf_voc_setup, char *encoder_name, char *decoder_name);
void rtc_get_dialog_info(aud_intf_voc_setup_t *aud_intf_voc_setup, char *encoder_name, char *decoder_name);
void rtc_websocket_rx_data_clean(void);


#ifdef __cplusplus
}
#endif
#endif /* __AGORA_RTC_H__ */
