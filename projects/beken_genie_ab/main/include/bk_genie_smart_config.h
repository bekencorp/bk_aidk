#ifndef __BK_GENIE_SMART_CONFIG_H__
#define __BK_GENIE_SMART_CONFIG_H__

int bk_agora_ai_agent_start(char *channel);
int bk_genie_smart_config_init(void);
void bk_genie_smart_config_cli(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);
#endif