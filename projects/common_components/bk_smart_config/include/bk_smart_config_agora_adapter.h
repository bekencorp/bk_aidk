#ifndef __BK_SMART_CONFIG_AGORA_ADAPTER_H__
#define __BK_SMART_CONFIG_AGORA_ADAPTER_H__

typedef struct
{
    uint8_t valid;
    char appid[33];
    char channel_name[128];
} bk_sconf_agent_info_t;

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

#if CONFIG_BK_DEV_STARTUP_AGENT
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
void bk_app_notify_wakeup_event(void);
uint16_t bk_sconf_send_agent_info(char *payload, uint16_t max_len);
void  bk_sconf_prase_agent_info(char *payload, uint8_t reset);
void bk_sconf_erase_agent_info(void);
int bk_sconf_save_agent_info(char *appid, char *channel_name);
int bk_sconf_get_agent_info(bk_sconf_agent_info_t *info);
int bk_sconf_wakeup_agent(uint8_t reset);
int bk_sconf_upate_agent_info(char *update_info);
void bk_sconf_begin_to_switch_ir_mode(void);
int bk_sconf_post_nfc_id(uint8_t *nfc_id);
#if CONFIG_ENABLE_AGORA_DATASTREAM
int bk_sconf_init_datastream_resource();
#endif
#endif
