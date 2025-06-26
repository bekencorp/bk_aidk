#ifndef __BK_SMART_CONFIG_H__
#define __BK_SMART_CONFIG_H__

#if CONFIG_AGORA_IOT_SDK
#include "bk_smart_config_agora_adapter.h"
#endif

#if CONFIG_BK_WSS_TRANS
#include "bk_smart_config_wss_adapter.h"
#endif

#if CONFIG_VOLC_RTC_EN
#include "bk_smart_config_volc_adapter.h"
#endif

#if CONFIG_LINGXIN_AI_EN
#include "bk_smart_config_lingxin_adapter.h"
#endif

typedef struct bk_fast_connect_d
{
	uint8_t flag;		//to check if ssid/pwd saved in easy flash is valid, default 0x70
					//bit[0]:write sta deault info;bit[1]:write ap deault info
	uint8_t sta_ssid[33];
	uint8_t sta_pwd[65];
	uint8_t ap_ssid[33];
	uint8_t ap_pwd[65];
	uint8_t ap_channel;
}BK_FAST_CONNECT_D;

void network_reconnect_start_timeout_check(uint32_t timeout);
void network_reconnect_stop_timeout_check(void);
int demo_network_auto_reconnect(bool val);
int bk_smart_config_init(void);
void bk_smart_config_cli(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
void event_handler_deinit(void);
void bk_sconf_prepare_for_smart_config(void);
void bk_sconf_start_to_config_network(void);
void bk_sconf_trans_start(void);
void bk_sconf_trans_stop(void);
int bk_sconf_is_net_pan_configured(void);
uint8_t bk_sconf_get_supported_engine(void);
uint8_t * bk_sconf_get_supported_network(uint8_t *len);
int bk_sconf_sync_flash(void);
void bk_sconf_sync_flash_safely(void);
#endif
