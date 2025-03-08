#ifndef __BK_GENIE_SMART_CONFIG_H__
#define __BK_GENIE_SMART_CONFIG_H__

typedef struct
{
    uint8_t valid;
    char appid[33];
    char channel_name[128];
} bk_genie_agent_info_t;

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

void network_pair_start_timeout_check(void);
void network_pair_stop_timeout_check(void);
int bk_agora_ai_agent_start(char *channel);
int bk_genie_smart_config_init(void);
void bk_genie_smart_config_cli(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
void event_handler_deinit(void);
void bk_genie_erase_agent_info(void);
int bk_genie_save_agent_info(char *appid, char *channel_name);
int bk_genie_get_agent_info(bk_genie_agent_info_t *info);
void bk_genie_prepare_for_smart_config(void);
int bk_genie_wakeup_agent(void);
#endif
