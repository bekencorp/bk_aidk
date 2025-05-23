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

typedef enum
{
	HTTP_STATUS_SUCCESS = 200,
	HTTP_STATUS_PARAM_ERROR = 400,
	HTTP_STATUS_MAX_AGENT_UPTIME_EXCEEDED = 403,
	HTTP_STATUS_TRIAL_LIMIT_EXCEEDED = 404,
	HTTP_STATUS_DEVICE_REMOVED = 405,
	HTTP_STATUS_AGENT_START_FAILED = 406,
}agent_status_code;

#if CONFIG_ENABLE_AGORA_DATASTREAM
typedef struct {
	char *data;
}bk_agora_ai_data_stream_t;
#endif

#if CONFIG_BK_AGORA_DEV_STARTUP_AGENT
typedef struct {
	char *url;
	char *api_key;
	char *system_messages;
	char *params;
	int max_history;
	char *input_modalities;
	char *output_modalities;
	char *greeting_message;
	char *failure_message;
}agora_custom_llm_t;

typedef struct {
	char *channel;
	agora_custom_llm_t *custom_llm;
}agora_ai_agent_start_conf_t;

#define BK_AGORA_AGENT_DEFAULT_CONFIG() {\
	.channel = NULL,\
	.custom_llm = NULL,\
}

typedef enum {
	OPEN_AI_AGENT,
	DOUBAO_AGENT,
	MAX_AGENT
}agent_type_t;

agora_custom_llm_t * custom_llm_default_conf(agent_type_t agent);
void custom_llm_default_conf_free(agora_custom_llm_t *custom_llm);
int bk_agora_ai_agent_start(agora_ai_agent_start_conf_t *agent_conf, agent_type_t agent);
int bk_agora_ai_agent_stop(char* agentID);
#endif

void network_reconnect_start_timeout_check(uint32_t timeout);
void network_reconnect_stop_timeout_check(void);
int demo_network_auto_reconnect(bool val);
int bk_genie_smart_config_init(void);
void bk_genie_smart_config_cli(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
void event_handler_deinit(void);
void bk_genie_erase_agent_info(void);
int bk_genie_save_agent_info(char *appid, char *channel_name);
int bk_genie_get_agent_info(bk_genie_agent_info_t *info);
void bk_genie_prepare_for_smart_config(void);
int bk_genie_wakeup_agent(uint8_t reset);
int bk_genie_upate_agent_info(char *update_info);
int bk_genie_is_net_pan_configured(void);
int bk_genie_post_nfc_id(uint8_t *nfc_id);
#if CONFIG_ENABLE_AGORA_DATASTREAM
int bk_genie_init_datastream_resource();
#endif
#endif
