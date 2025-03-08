#ifndef __BK_GENIE_SMART_CONFIG_H__
#define __BK_GENIE_SMART_CONFIG_H__

typedef struct
{
    uint8_t valid;
    char appid[33];
    char channel_name[128];
} bk_genie_agent_info_t;

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