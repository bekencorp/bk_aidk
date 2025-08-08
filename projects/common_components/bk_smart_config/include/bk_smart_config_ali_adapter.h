#ifndef __BK_SMART_CONFIG_ALI_ADAPTER_H__
#define __BK_SMART_CONFIG_ALI_ADAPTER_H__
typedef enum
{
	HTTP_STATUS_SUCCESS = 200,
}status_code;
void bk_app_notify_wakeup_event(void);
int bk_sconf_wakeup_agent(uint8_t reset);
void bk_sconf_begin_to_switch_ir_mode(void);
int bk_sconf_post_nfc_id(uint8_t *nfc_id);
uint16_t bk_sconf_send_agent_info(char *payload, uint16_t max_len);
void  bk_sconf_prase_agent_info(char *payload, uint8_t reset);
#endif