// Copyright 2020-2025 Beken
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

#include <common/sys_config.h>
#include <components/log.h>
#include <modules/wifi.h>
#include <components/netif.h>
#include <components/event.h>
#include <string.h>
#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include "components/webclient.h"
#include "cJSON.h"
#include "components/bk_uid.h"
#include "bk_genie_comm.h"
#include "wifi_boarding_utils.h"
#include "bk_genie_smart_config.h"
#include "pan_service.h"
#include "led_blink.h"
#include "app_event.h"
#include "boarding_service.h"
#include "components/bluetooth/bk_dm_bluetooth.h"
#include "bk_factory_config.h"
#include "driver/trng.h"

#define TAG "bk_sconf"
#define RCV_BUF_SIZE            256
#define SEND_HEADER_SIZE           1024
#define POST_DATA_MAX_SIZE  1024*2
#define MAX_URL_LEN         256
extern char *app_id_record;
bool smart_config_running = false;
char *app_id_record = NULL;
char *channel_name_record = NULL;
static beken2_timer_t network_reconnect_tmr = {0};
extern bool agora_runing;
uint8_t network_disc_evt_posted = 0;
bool first_time_for_network_provisioning = true;

#if  CONFIG_BK_AGORA_DEV_STARTUP_AGENT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <os/mem.h>
#include <os/str.h>
#include "agora_config.h"
#include "components/webclient.h"
#include "cJSON.h"

#define AGORA_OPENAI_URL "https://api.agora.io/api/conversational-ai-agent/v2/projects/"
#define AGORA_DOUBAO_URL "https://api.agora.io/cn/api/conversational-ai-agent/v2/projects/"

#define AGORA_DEBUG_APPID "xxx"	//need to be replaced by customers
#define AGORA_DEBUG_AUTH "xxx"		//need to be replaced by customers

#define BK_AGORA_JOIN "join"
#define BK_AGORA_STOP "leave"

/*custom_llm default config*/
/*open ai*/
#define CUSTOM_LLM_DEFAULT_OPENAI_URL "https://api.openai.com/v1/chat/completions"
#define CUSTOM_LLM_DEFAULT_OPENAI_TOKEN "xxx"		//need to be replaced by customers
#define CUSTOM_LLM_DEFAULT_OPENAI_PROMPT "You are a helpful voice agent that will get asr result from user speech. I will ask you question. The response should be helpful and informative. The response should be in a friendly and professional tone. The response should be in English. The response should be generated in a timely manner."
#define CUSTOM_LLM_DEFAULT_OPENAI_MODEL "gpt-4o-mini"
#define CUSTOM_LLM_DEFAULT_OPENAI_GREETING "Merry Christmas, how can I assist you today?"
/*doubao*/
#define CUSTOM_LLM_DEFAULT_DOUBAO_URL "https://ark.cn-beijing.volces.com/api/v3/chat/completions"
#define CUSTOM_LLM_DEFAULT_DOUBAO_TOKEN "xxx"		//need to be replaced by customers
#define CUSTOM_LLM_DEFAULT_DOUBAO_PROMPT  "����һ������ò��AI��������ʹ�����硰�õġ�����û���⡱������Ǹ���������Ĵʿ�ʼ��Ļش�"
#define CUSTOM_LLM_DEFAULT_DOUBAO_MODEL "ep-20250213161421-v9m5m"
#define CUSTOM_LLM_DEFAULT_DOUBAO_GREETING "�´����֣���ʲô���԰�����"

agent_type_t agent_record = DOUBAO_AGENT;
char *agent_id_record = NULL;

char tts_str_openai[] = "\"tts\": {"
	"\"vendor\": \"microsoft\","
	"\"params\": {"
		"\"key\": \"xxx\","				//need to be replaced by customers
		"\"region\": \"koreacentral\","
		"\"voice_name\": \"en-US-AndrewMultilingualNeural\","
		"\"vol\": 10"
       "}"
 "},";

char tts_str_doubao[] = "\"tts\": {"
	"\"vendor\": \"bytedance\","
	"\"params\": {"
		"\"token\": \"xxx\","		//need to be replaced by customers
		"\"app_id\": \"xxx\","		//need to be replaced by customers
		"\"cluster\": \"volcano_tts\","
		"\"speed_ratio\": 1.0,"
		"\"volume_ratio\": 0.6,"
		"\"pitch_ratio\": 1.0,"
		"\"emotion\": \"happy\""
       "}"
 "},";

char vad_str_openai[] = "\"asr\": {"
            "\"language\": \"en-US\","
            "\"vendor\": \"tencent\"}";
char vad_str_doubao[] = "\"asr\": {"
            "\"language\": \"zh-CN\","
            "\"vendor\": \"tencent\"}";

int bk_parse_agent_conf(agora_ai_agent_start_conf_t *agent_conf, char *post_data, agent_type_t agent)
{
	int len = 0;
	/*Begin*/
	/*name*/
	if (agent_conf->channel)
		len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "{\"name\":\"%s\",\r\n", agent_conf->channel);

	/*properties*/
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"properties\":{\r\n");

	if (agent_conf->channel)
		len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"channel\": \"%s\",\r\n", agent_conf->channel);
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"token\": \"\",\r\n");
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"agent_rtc_uid\": \"1234\",\r\n");
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"remote_rtc_uids\": [\"123\"],\r\n");
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"advanced_features\": {\"enable_bhvs\": true,\"enable_aivad\": false},\r\n");
#if CONFIG_USE_OPUS_CODEC
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"parameters\": {\"enable_dump\": true,\"output_audio_codec\": \"OPUS\"},\r\n");
#else
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"parameters\": {\"enable_dump\": true,\"output_audio_codec\": \"G722\"},\r\n");
#endif
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"enable_string_uid\": false,\r\n");
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"idle_timeout\": %d,\r\n", 300);	//5min
	if (agent_conf->custom_llm) {
		len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"llm\": {\r\n");
		if (agent_conf->custom_llm->url)
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"url\": \"%s\",\r\n", agent_conf->custom_llm->url);
		if (agent_conf->custom_llm->api_key)
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"api_key\": \"%s\",\r\n", agent_conf->custom_llm->api_key);
		len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"max_history\": %d,\r\n", agent_conf->custom_llm->max_history);
		if (agent == OPEN_AI_AGENT) {
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"system_messages\": [{\"role\": \"system\",\"content\": \"You are a helpful chatbot.\"}],\r\n");
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"greeting_message\": \"Merry Christmas, how can I assist you today?\",\r\n");
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"failure_message\": \"I am sorry!\",\r\n");
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"params\": {\"model\": \"gpt-4o-mini\"}},\r\n");
		} else {
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"system_messages\": [{\"role\": \"system\",\"content\": \"你是一个有礼貌的AI助理。\"}],\r\n");
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"greeting_message\": \"新春快乐，有什么可以帮�?\",\r\n");
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"failure_message\": \"很抱歉。\",\r\n");
			len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "\"params\": {\"model\": \"ep-20250213161421-v9m5m\"}},\r\n");
		}
	}
	if (agent == OPEN_AI_AGENT) {
		len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "%s\r\t%s\r\n", tts_str_openai, vad_str_openai);
	} else {
		len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "%s\r\t%s\r\n", tts_str_doubao, vad_str_doubao);
	}

	/*End*/
	len += os_snprintf(post_data + len, POST_DATA_MAX_SIZE, "}}\r\n");

	BK_LOGI(TAG,"%s, %s\r\n", __func__, post_data);

	return len;
}

int bk_agora_ai_agent_start_rsp_parse(char *text)
{
	cJSON *json = NULL;
	__maybe_unused char *StatusCode = NULL, *state, *detail, *reason;
	__maybe_unused int create_ts;

	json = cJSON_Parse(text);
	if (!json)
	{
	    BK_LOGE(TAG,"Error before: [%s]\n", cJSON_GetErrorPtr());
	    return BK_FAIL;
	}

	cJSON *code = cJSON_GetObjectItem(json, "status");
	if (code && ((code->type & 0xFF) == cJSON_String)) {
	    StatusCode = os_strdup(code->valuestring);
	}
	else
	{
	    BK_LOGE(TAG,"[Error] not find statusCode\n");
	    goto fail;
	}
	BK_LOGE(TAG,"%s, StatusCode:%s\r\n", __func__, StatusCode);

	cJSON *agentid = cJSON_GetObjectItem(json, "agent_id");
	if (agentid && ((agentid->type & 0xFF) == cJSON_String)) {
		agent_id_record = os_strdup(agentid->valuestring);
		BK_LOGI(TAG,"agent_id:%s\r\n", agent_id_record);
	}
	cJSON_Delete(json);

    return BK_OK;

fail:
    if (json)
    {
        cJSON_Delete(json);
    }

    return BK_FAIL;
}

/******************Agent start******************
Method:	POST
URL:		https://api.agora.io/cn/api/conversational-ai-agent/v1/projects/{appid}/join
More details, please check docs locate in /components/docs/agora_ai_agent/
****************************************************************/
int bk_agora_ai_agent_start(agora_ai_agent_start_conf_t *agent_conf, agent_type_t agent)
{
	struct webclient_session* session = NULL;
	char *buffer = NULL, *post_data = NULL;
	char generate_url[256] = {0};
	int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0;

	/* create webclient session and set header response size */
	session = webclient_session_create(SEND_HEADER_SIZE);
	if (session == NULL) {
	    goto __exit;
	}
	agent_record = agent;
	/*Generate https url*/
	if (agent == OPEN_AI_AGENT)
		url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s%s/%s", AGORA_OPENAI_URL, AGORA_DEBUG_APPID, BK_AGORA_JOIN);
	else if (agent == DOUBAO_AGENT)
		url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s%s/%s", AGORA_DOUBAO_URL, AGORA_DEBUG_APPID, BK_AGORA_JOIN);
	if ((url_len < 0) || (url_len >= MAX_URL_LEN)) {
		BK_LOGE(TAG,"URL len overflow\r\n");
		return BK_FAIL;
	}

	/*Generate data*/
	post_data = os_malloc(POST_DATA_MAX_SIZE);
	if (post_data == NULL) {
		BK_LOGE(TAG,"no memory for post_data buffer\n");
		goto __exit;
	}
	os_memset(post_data, 0, POST_DATA_MAX_SIZE);
	data_len = bk_parse_agent_conf(agent_conf, post_data, agent);

	/*Generate https header*/
	webclient_header_fields_add(session, "Content-Length: %d\r\n", os_strlen(post_data));
	webclient_header_fields_add(session, "Content-Type: application/json\r\n");
	webclient_header_fields_add(session, "Authorization: Basic %s\r\n", AGORA_DEBUG_AUTH);

	buffer = (char *) web_malloc(RCV_BUF_SIZE);
	if (buffer == NULL) {
		BK_LOGE(TAG,"no memory for receive response buffer.\n");
		goto __exit;
	}
	os_memset(buffer, 0, RCV_BUF_SIZE);

	/* send POST request by default header */
	if ((resp_status = webclient_post(session, generate_url, post_data, data_len)) != 200) {
		BK_LOGE(TAG,"webclient POST request failed, response(%d) error.\n", resp_status);
	}

	BK_LOGI(TAG,"webclient post response data: \n");
	do {
		bytes_read = webclient_read(session, buffer, RCV_BUF_SIZE);
		if (bytes_read > 0)
		{
			break;
		}
	} while (1);
	BK_LOGI(TAG,"bytes_read: %d\n", bytes_read);

	BK_LOGI(TAG,"buffer %s.\n", buffer);

	resp_status = bk_agora_ai_agent_start_rsp_parse(buffer);

__exit:
	if (session) {
		webclient_close(session);
	}

	if (buffer) {
		web_free(buffer);
	}

	if (post_data) {
		os_free(post_data);
	}

    return BK_OK;
}

int bk_agora_ai_agent_stop_rsp_parse(char *text)
{
	cJSON *json = NULL;
	__maybe_unused char *StatusCode = NULL, *state, *detail, *reason;
	__maybe_unused int create_ts;

	json = cJSON_Parse(text);
	if (!json)
	{
	    BK_LOGE(TAG,"Error before: [%s]\n", cJSON_GetErrorPtr());
	    return BK_FAIL;
	}


	cJSON *code = cJSON_GetObjectItem(json, "statusCode");
	if (code && ((code->type & 0xFF) == cJSON_String)) {
	    StatusCode = os_strdup(code->valuestring);
	}
	else
	{
	    BK_LOGE(TAG,"[Error] not find statusCode\n");
	    goto fail;
	}
	BK_LOGE(TAG,"%s, StatusCode:%s\r\n", __func__, StatusCode);

    cJSON_Delete(json);

    return BK_OK;

fail:
    if (json)
    {
        cJSON_Delete(json);
    }

    return BK_FAIL;
}

/******************agent stop******************
Method:	POST
URL:		https://api.agora.io/cn/api/conversational-ai-agent/v1/projects/{appid}/agents/{agentId}/leave
More details, please check docs locate in /components/docs/agora_ai_agent/
****************************************************************/
int bk_agora_ai_agent_stop(char* agentID)
{
	struct webclient_session* session = NULL;
	char *buffer = NULL ,*post_data = NULL;
	char generate_url[256] = {0};
	int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0;

	/* create webclient session and set header response size */
	session = webclient_session_create(SEND_HEADER_SIZE);
	if (session == NULL) {
	    goto __exit;
	}

	/*Generate https url*/
	if (agent_record == OPEN_AI_AGENT)
		url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s%s/agents/%s/%s", AGORA_OPENAI_URL, AGORA_DEBUG_APPID, agentID, BK_AGORA_STOP);
	else if (agent_record == DOUBAO_AGENT)
		url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s%s/agents/%s/%s", AGORA_DOUBAO_URL, AGORA_DEBUG_APPID, agentID, BK_AGORA_STOP);
	if ((url_len < 0) || (url_len >= MAX_URL_LEN)) {
		BK_LOGE(TAG,"URL len overflow\r\n");
		return BK_FAIL;
	}

	/*Generate https header*/
	webclient_header_fields_add(session, "Authorization: Basic %s\r\n", AGORA_DEBUG_AUTH);

	buffer = (char *) web_malloc(RCV_BUF_SIZE);
	if (buffer == NULL) {
		BK_LOGE(TAG,"no memory for receive response buffer.\n");
		goto __exit;
	}
	os_memset(buffer, 0, RCV_BUF_SIZE);

	/* send POST request by default header */
	if ((resp_status = webclient_post(session, generate_url, post_data, data_len)) != 200) {
		BK_LOGE(TAG,"webclient POST request failed, response(%d) error.\n", resp_status);
	}

	BK_LOGI(TAG,"webclient post response data: \n");
	do {
		bytes_read = webclient_read(session, buffer, RCV_BUF_SIZE);
		if (bytes_read > 0)
		{
			break;
		}
	} while (1);
	BK_LOGI(TAG,"bytes_read: %d\n", bytes_read);

	BK_LOGI(TAG,"buffer %s.\n", buffer);

	resp_status = bk_agora_ai_agent_stop_rsp_parse(buffer);

__exit:
	if (session) {
		webclient_close(session);
	}

	if (buffer) {
		web_free(buffer);
	}

	if (post_data) {
		os_free(post_data);
	}

	if (agent_id_record)
		os_free(agent_id_record);

    return BK_OK;
}

agora_custom_llm_t * custom_llm_default_conf(agent_type_t agent)
{
	agora_custom_llm_t *custom_llm;

	custom_llm = os_zalloc(sizeof(agora_custom_llm_t));
	BK_LOGI(TAG,"custom_llm %p\r\n", custom_llm);

	if (agent == OPEN_AI_AGENT) {
		custom_llm->url = os_strdup(CUSTOM_LLM_DEFAULT_OPENAI_URL);
		custom_llm->api_key= os_strdup(CUSTOM_LLM_DEFAULT_OPENAI_TOKEN);
		custom_llm->max_history = 10;
	} else {
		custom_llm->url = os_strdup(CUSTOM_LLM_DEFAULT_DOUBAO_URL);
		custom_llm->api_key= os_strdup(CUSTOM_LLM_DEFAULT_DOUBAO_TOKEN);
		custom_llm->max_history = 10;
	}

	return custom_llm;
}

void custom_llm_default_conf_free(agora_custom_llm_t *custom_llm)
{
	if (custom_llm) {
		if (custom_llm->url)
			os_free(custom_llm->url);
		if (custom_llm->api_key)
			os_free(custom_llm->api_key);
			os_free(custom_llm);
	}
}
#endif

void network_reconnect_check_status(void)
{
    BK_LOGI(TAG,"reconnect timeout!\n");
    if (network_disc_evt_posted == 0) {
        app_event_send_msg(APP_EVT_RECONNECT_NETWORK_FAIL, 0);
        network_disc_evt_posted = 1;
    }
}

void network_reconnect_start_timeout_check(uint32_t timeout)
{
  bk_err_t err = kNoErr;
  uint32_t clk_time;

  clk_time = timeout*1000;		//timeout unit: seconds

  if (rtos_is_oneshot_timer_init(&network_reconnect_tmr)) {
     BK_LOGI(TAG,"network provisioning status timer reload\n");
    rtos_oneshot_reload_timer(&network_reconnect_tmr);
  } else {
    err = rtos_init_oneshot_timer(&network_reconnect_tmr, clk_time, (timer_2handler_t)network_reconnect_check_status, NULL, NULL);
    BK_ASSERT(kNoErr == err);

    err = rtos_start_oneshot_timer(&network_reconnect_tmr);
    BK_ASSERT(kNoErr == err);
     BK_LOGI(TAG,"network provisioning status timer:%d\n", clk_time);
  }

  return;
}


void network_reconnect_stop_timeout_check(void)
{
  bk_err_t ret = kNoErr;

  if (rtos_is_oneshot_timer_init(&network_reconnect_tmr)) {
    if (rtos_is_oneshot_timer_running(&network_reconnect_tmr)) {
      ret = rtos_stop_oneshot_timer(&network_reconnect_tmr);
      BK_ASSERT(kNoErr == ret);
    }

    ret = rtos_deinit_oneshot_timer(&network_reconnect_tmr);
    BK_ASSERT(kNoErr == ret);
  }
}

int is_wifi_sta_auto_restart_info_saved(void)
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
	if (info.flag == 0x71l)
		return 0;
	else
		return 1;
}

int bk_genie_is_wifi_sta_configured(void)
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
	if ((info.flag & 0x71l) == 0x71l)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int bk_genie_is_net_pan_configured(void)
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
#if CONFIG_NET_PAN
	if ((info.flag & 0x74l) == 0x74l)
	{
		return 1;
	}
	else
#endif
	{
		return 0;
	}
}

void demo_erase_network_auto_reconnect_info(void)
{
	BK_FAST_CONNECT_D info_tmp = {0};

	bk_config_write("d_network_id", (const void *)&info_tmp, sizeof(BK_FAST_CONNECT_D));
}

extern int demo_sta_app_init(char *oob_ssid, char *connect_key);
extern int demo_softap_app_init(char *ap_ssid, char *ap_key, char *ap_channel);
int demo_network_auto_reconnect(bool val)	//val true means from disconnect to reconnecting
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
	/*0x01110001:sta, 0x01110010:softap, 0x01110100:pan*/
	if ((info.flag & 0x71l) == 0x71l) {
		if (val == false) {
			network_reconnect_stop_timeout_check();
			network_reconnect_start_timeout_check(50);    //50s
			app_event_send_msg(APP_EVT_RECONNECT_NETWORK, 0);
			network_disc_evt_posted = 0;
		}
		demo_sta_app_init((char *)info.sta_ssid, (char *)info.sta_pwd);
		return 0x71l;
	}
	if ((info.flag & 0x72l)  == 0x72l) {
		demo_softap_app_init((char *)info.ap_ssid, (char *)info.ap_pwd, NULL);
		return 0x72l;
	}
#if CONFIG_NET_PAN
	if ((info.flag & 0x74l) == 0x74l) {
		if (val == false) {
			network_reconnect_stop_timeout_check();
			network_reconnect_start_timeout_check(50);    //50s
			app_event_send_msg(APP_EVT_RECONNECT_NETWORK, 0);
			network_disc_evt_posted = 0;
		}
		pan_service_init();
		bt_start_pan_reconnect();
		return 0x74l;
	}
#endif
#if CONFIG_BK_MODEM
	if ((info.flag & 0x78l) == 0x78l) {
		if (val == false) {
			network_reconnect_stop_timeout_check();
			network_reconnect_start_timeout_check(50);    //50s
			app_event_send_msg(APP_EVT_RECONNECT_NETWORK, 0);
			network_disc_evt_posted = 0;
		}
extern bk_err_t bk_modem_init(void);
		bk_modem_init();
		return 0x78l;
	}
#endif
	return 0;
}

int demo_save_network_auto_restart_info(netif_if_t type, void *val)
{
	BK_FAST_CONNECT_D info_tmp = {0};
	__maybe_unused wifi_ap_config_t *ap_config = NULL;
	__maybe_unused wifi_sta_config_t *sta_config = NULL;

	bk_config_read("d_network_id", (void *)&info_tmp, sizeof(BK_FAST_CONNECT_D));
	if ((info_tmp.flag & 0xf0l) != 0x70l) {
		BK_LOGI(TAG, "erase network provisioning info, %x\r\n", info_tmp.flag);
		info_tmp.flag = 0x70l;
	}
	if (type == NETIF_IF_STA) {
		info_tmp.flag |= 0x71l;
		sta_config = (wifi_sta_config_t *)val;
		os_memset((char *)info_tmp.sta_ssid, 0x0, 33);
		os_memset((char *)info_tmp.sta_pwd, 0x0, 65);
		os_strcpy((char *)info_tmp.sta_ssid, (char *)sta_config->ssid);
		os_strcpy((char *)info_tmp.sta_pwd, (char *)sta_config->password);
	} else if (type == NETIF_IF_AP) {
		info_tmp.flag |= 0x72l;
		ap_config = (wifi_ap_config_t *)val;
		os_memset((char *)info_tmp.ap_ssid, 0x0, 33);
		os_memset((char *)info_tmp.ap_pwd, 0x0, 65);
		os_strcpy((char *)info_tmp.ap_ssid, (char *)ap_config->ssid);
		os_strcpy((char *)info_tmp.ap_pwd, (char *)ap_config->password);
#if CONFIG_NET_PAN
	} else if (type == NETIF_IF_PAN) {
		info_tmp.flag |= 0x74l;
#endif
#if CONFIG_BK_MODEM
	} else if (type == NETIF_IF_PPP) {
		info_tmp.flag |= 0x78l;
#endif
	} else
		return -1;
	bk_config_write("d_network_id", (const void *)&info_tmp, sizeof(BK_FAST_CONNECT_D));

	return 0;
}

#if CONFIG_NET_PAN
static int bk_genie_reselect_pan(void)
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
	if (info.flag & 0x74l) {
		BK_LOGI(TAG, "%s\r\n", __func__);
		bk_wifi_sta_stop();
		bk_bluetooth_init();
		pan_service_init();
		bt_start_pan_reconnect();
		return 1;
	}
	return 0;
}
#endif

extern void agora_auto_run(void);
static int bk_genie_sconf_netif_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    netif_event_got_ip4_t *got_ip;
    bk_genie_msg_t msg;
    __maybe_unused wifi_sta_config_t sta_config = {0};
    __maybe_unused bk_genie_agent_info_t info = {0};

    switch (event_id)
    {
        case EVENT_NETIF_GOT_IP4:
            network_disc_evt_posted = 0;
            got_ip = (netif_event_got_ip4_t *)event_data;
            BK_LOGI(TAG, "netif_idx %d\r got ip\n", got_ip->netif_if);
#if CONFIG_BK_MODEM
            {
             extern void ping_start(char* target_name, uint32_t times, size_t size);
             ping_start("baidu.com", 4, 0);
            }
#endif

            if (smart_config_running)
            {
                app_event_send_msg(APP_EVT_NETWORK_PROVISIONING_SUCCESS, 0);
                bk_wifi_sta_get_config(&sta_config);
                demo_save_network_auto_restart_info(got_ip->netif_if, &sta_config);
                //inform beken apk netif got ip
                msg.event = DBEVT_NETWORK_CONNECTED;
                msg.param = got_ip->netif_if;
                bk_genie_send_msg(&msg);
#if CONFIG_STA_AUTO_RECONNECT
                if (!first_time_for_network_provisioning) {
                    BK_LOGI(TAG, "first_time_for_network_provisioning\r\n");
                    goto skip_agent_request;
                }
#endif
#if CONFIG_BK_AGORA_DEV_STARTUP_AGENT
                msg.event = DBEVT_START_AGORA_AGENT_ON_DEV;
                bk_genie_send_msg(&msg);
#else
                msg.event = DBEVT_START_AGORA_AGENT_START;
                bk_genie_send_msg(&msg);
#endif

            }
            else
            {
                app_event_send_msg(APP_EVT_RECONNECT_NETWORK_SUCCESS, 0);
#if CONFIG_STA_AUTO_RECONNECT
skip_agent_request:
#endif
#if CONFIG_BK_AGORA_DEV_STARTUP_AGENT
                agora_auto_run();
#else
                if (bk_genie_get_agent_info(&info) == 0)
                {
                    if (info.valid != 1)
                    {
                        break;
                    }
                    if (app_id_record)
                    {
                        os_free(app_id_record);
                    }
                    if (channel_name_record)
                    {
                        os_free(channel_name_record);
                    }
                    app_id_record = os_strdup(info.appid);
                    channel_name_record = os_strdup(info.channel_name);
                    BK_LOGI(TAG, "%s, %s\r\n", app_id_record, channel_name_record);
                    agora_auto_run();
                }
#endif
            }

            break;
        default:
            BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}

static int bk_genie_sconf_wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    wifi_event_sta_disconnected_t *sta_disconnected;
    wifi_event_sta_connected_t *sta_connected;
    bk_genie_msg_t msg;

    switch (event_id)
    {
        case EVENT_WIFI_STA_CONNECTED:
            sta_connected = (wifi_event_sta_connected_t *)event_data;
            BK_LOGI(TAG, "STA connected to %s\n", sta_connected->ssid);
            break;

        case EVENT_WIFI_STA_DISCONNECTED:
            sta_disconnected = (wifi_event_sta_disconnected_t *)event_data;
            BK_LOGI(TAG, "STA disconnected, reason(%d)\n", sta_disconnected->disconnect_reason);
            /*drop local generated disconnect event by user*/
            if ((sta_disconnected->disconnect_reason == WIFI_REASON_DEAUTH_LEAVING &&
				sta_disconnected->local_generated == 1) ||
				(sta_disconnected->disconnect_reason == WIFI_REASON_RESERVED))
			break;
#if CONFIG_STA_AUTO_RECONNECT
            if (bk_genie_is_net_pan_configured() && !smart_config_running) {
#if CONFIG_NET_PAN
			bk_genie_reselect_pan();
#endif
            } else
#endif
            {
			if (network_disc_evt_posted == 0) {
				if (smart_config_running == false)
					app_event_send_msg(APP_EVT_RECONNECT_NETWORK_FAIL, 0);
				else {
					msg.event = DBEVT_WIFI_STATION_DISCONNECTED;
					bk_genie_send_msg(&msg);
					app_event_send_msg(APP_EVT_NETWORK_PROVISIONING_FAIL, 0);
				}
				network_disc_evt_posted = 1;
			}
#if CONFIG_STA_AUTO_RECONNECT
			bk_wifi_sta_connect();
#endif
            }
            break;

        default:
            BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}


void event_handler_init(void)
{
    BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, bk_genie_sconf_wifi_event_cb, NULL));
    BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_NETIF, EVENT_ID_ALL, bk_genie_sconf_netif_event_cb, NULL));
}

extern bk_err_t agora_stop(void);
void bk_genie_prepare_for_smart_config(void)
{
    smart_config_running = true;
#if CONFIG_STA_AUTO_RECONNECT
    first_time_for_network_provisioning = true;
#endif
    app_event_send_msg(APP_EVT_NETWORK_PROVISIONING, 0);
    network_reconnect_stop_timeout_check();
    agora_stop();
    bk_wifi_sta_stop();
#if CONFIG_BK_MODEM
extern bk_err_t bk_modem_deinit(void);
    bk_modem_deinit();
#endif
#if !CONFIG_STA_AUTO_RECONNECT
    demo_erase_network_auto_reconnect_info();
    bk_genie_erase_agent_info();
#endif
    bk_bt_enter_pairing_mode(0);

    extern bool ate_is_enabled(void);

    if (!ate_is_enabled())
    {
        bk_genie_boarding_init();
        wifi_boarding_adv_start();
    }
    else
    {
        BK_LOGW(TAG, "%s ATE is enable, ble will not enable!!!!!!\n", __func__);
    }

    //network_reconnect_start_timeout_check(300);	//5min
}

int bk_genie_smart_config_init(void)
{
    int flag;

    event_handler_init();
    flag = demo_network_auto_reconnect(false);

    if (flag != 0x71l
#if CONFIG_NET_PAN
        && flag != 0x74l
#endif
#if CONFIG_BK_MODEM
	 && flag != 0x78l
#endif
    ) {
        bk_genie_prepare_for_smart_config();
    }
    else
    {
#if CONFIG_NET_PAN
        if (flag != 0x74l)
#endif
        {
            bk_bluetooth_deinit();
        }
    }

    return 0;
}

extern void demo_wifi_erase_auto_restart_info(void);
void bk_genie_smart_config_cli(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    demo_erase_network_auto_reconnect_info();
}

void bk_genie_erase_agent_info(void)
{
    bk_genie_agent_info_t info_tmp = {0};

    bk_config_write("d_agent_info", (const void *)&info_tmp, sizeof(bk_genie_agent_info_t));
}

int bk_genie_save_agent_info(char *appid, char *channel_name)
{
    bk_genie_agent_info_t info_tmp = {0};

    info_tmp.valid = 1;
    os_strcpy(info_tmp.appid, appid);
    os_strcpy(info_tmp.channel_name, channel_name);
    bk_config_write("d_agent_info", (const void *)&info_tmp, sizeof(bk_genie_agent_info_t));

    return 0;
}

int bk_genie_get_agent_info(bk_genie_agent_info_t *info)
{
    bk_genie_agent_info_t info_tmp = {0};

    if (bk_config_read("d_agent_info", (void *)&info_tmp, sizeof(bk_genie_agent_info_t)) <= 0)
    {
        return -1;
    }
    os_memcpy(info, &info_tmp, sizeof(bk_genie_agent_info_t));
    return 0;
}

int bk_genie_rsp_parse_update(char *buffer)
{
	char *app_id_update = NULL;
	int code_update = 0;
	__maybe_unused bk_genie_agent_info_t info = {0};
	cJSON *json = cJSON_Parse(buffer);
	if (!json)
	{
		BK_LOGE(TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
		return BK_FAIL;
	}
	cJSON *code = cJSON_GetObjectItem(json, "code");
	if (code && ((code->type & 0xFF) == cJSON_Number)) {
		code_update = code->valueint;
		switch(code_update)
		{
			case HTTP_STATUS_SUCCESS:
				break;
			case HTTP_STATUS_TRIAL_LIMIT_EXCEEDED:
				BK_LOGI(TAG, "[UPDATE] the maximum number of agent expiriences has been reached, please contact armino_support@bekencorp.com for in-depth communication !\n");
			case HTTP_STATUS_PARAM_ERROR:
			case HTTP_STATUS_MAX_AGENT_UPTIME_EXCEEDED:
			case HTTP_STATUS_DEVICE_REMOVED:
			case HTTP_STATUS_AGENT_START_FAILED:
				cJSON_Delete(json);
				return BK_FAIL;
				break;

			default:
				cJSON_Delete(json);
				return BK_FAIL;
				break;
		}
	}
	else {
		BK_LOGE(TAG, "[Error] not find code msg\n");
		cJSON_Delete(json);
		return BK_FAIL;
	}

	cJSON *data = cJSON_GetObjectItem(json, "data");
	if (data)
	{
		cJSON *app_id = cJSON_GetObjectItem(data, "app_id");
		if (app_id && ((app_id->type & 0xFF) == cJSON_String)) {
			app_id_update = os_strdup(app_id->valuestring);
			BK_LOGI(TAG, "[UPDATE] appid:%s size:%d\n", app_id_update, strlen(app_id_update));
		}
		else {
			BK_LOGE(TAG, "[Error] not find app_id msg\n");
			cJSON_Delete(json);
			return BK_FAIL;
		}
	}
	else
	{
		BK_LOGE(TAG, "[Error] not find data msg\n");
		cJSON_Delete(json);
		return BK_FAIL;
	}
	cJSON_Delete(json);

	if (bk_genie_get_agent_info(&info) == 0)
	{
		if (info.valid != 1)
		{
			return BK_FAIL;
		}
		BK_LOGI(TAG, "[ORGINAL] appid:%s size:%d\r\n", info.appid, strlen(info.appid));
	}

	if (app_id_update && info.appid && (strcmp(app_id_update, info.appid)!=0))
	{
		BK_LOGI(TAG, "need update app id\n");
		if (app_id_record)
		{
			os_free(app_id_record);
		}
		app_id_record = os_strdup(app_id_update);
		bk_genie_save_agent_info(app_id_record, channel_name_record);
	}
	if (app_id_update)
		os_free(app_id_update);
	return BK_OK;
}

extern char *bk_get_bk_server_url(void);
int bk_genie_wakeup_agent(void)
{
#if CONFIG_BK_AGORA_DEV_STARTUP_AGENT
	agora_ai_agent_start_conf_t agent_conf = BK_AGORA_AGENT_DEFAULT_CONFIG();
	__maybe_unused agent_type_t agent_type = DOUBAO_AGENT;
	unsigned char uid[32] = {0};
	char uid_str[65] = {0}, chan_name[65] = {0};
	int chan_len;

	bk_uid_get_data(uid);
	for (int i = 0; i < 24; i++)
	{
		sprintf(uid_str + i * 2, "%02x", uid[i]);
	}
	if (agent_type == OPEN_AI_AGENT)
		chan_len = os_snprintf(chan_name, 65, "Openai_%s", uid_str);
	else
		chan_len = os_snprintf(chan_name, 65, "Doubao_%s", uid_str);
	agent_conf.channel = os_zalloc(chan_len+1);
	os_strcpy(agent_conf.channel, chan_name);

	agent_conf.custom_llm = custom_llm_default_conf(agent_type);
	bk_agora_ai_agent_start(&agent_conf, agent_type);

	if (agent_conf.channel)
		os_free(agent_conf.channel);
	custom_llm_default_conf_free(agent_conf.custom_llm);
extern char *app_id_record;
extern char *channel_name_record;
	if (app_id_record)
	{
	    os_free(app_id_record);
	}
	app_id_record = os_strdup(AGORA_DEBUG_APPID);
	if (channel_name_record)
	{
	    os_free(channel_name_record);
	}
	channel_name_record = os_strdup(chan_name);

	return 0;
#else
    struct webclient_session *session = NULL;
    char *buffer = NULL, *post_data = NULL;
    char generate_url[256] = {0};
    int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0, ret = 0;
    uint32_t rand_flag = 0;

    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == NULL)
    {
        ret = -1;
        goto __exit;
    }

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s", bk_get_bk_server_url());
    if ((url_len < 0) || (url_len >= MAX_URL_LEN))
    {
        BK_LOGE(TAG, "URL len overflow\r\n");
        ret = -1;
        return ret;
    }

    /*Generate data*/
    post_data = os_malloc(POST_DATA_MAX_SIZE);
    if (post_data == NULL)
    {
        ret = -1;
        BK_LOGE(TAG, "no memory for post_data buffer\n");
        goto __exit;
    }
    os_memset(post_data, 0, POST_DATA_MAX_SIZE);

    data_len = os_snprintf(post_data, POST_DATA_MAX_SIZE, "{\"channel\":\"%s\",", channel_name_record);
    rand_flag = bk_rand();
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"rand_flag\":\"%u\"}", rand_flag);
    BK_LOGI(TAG, "%s, %s\r\n", __func__, post_data);

    webclient_header_fields_add(session, "Content-Length: %d\r\n", os_strlen(post_data));
    webclient_header_fields_add(session, "Content-Type: application/json\r\n");

    buffer = (char *) web_malloc(RCV_BUF_SIZE);
    if (buffer == NULL)
    {
        ret = -1;
        BK_LOGE(TAG, "no memory for receive response buffer.\n");
        goto __exit;
    }
    os_memset(buffer, 0, RCV_BUF_SIZE);

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, generate_url, post_data, data_len)) != 200)
    {
        ret = -1;
        BK_LOGE(TAG, "webclient POST request failed, response(%d) error.\n", resp_status);
        goto __exit;
    }

    BK_LOGI(TAG, "webclient post response data: \n");
    do
    {
        bytes_read = webclient_read(session, buffer, RCV_BUF_SIZE);
        if (bytes_read > 0)
        {
            break;
        }
    }
    while (1);
    BK_LOGI(TAG, "bytes_read: %d\n", bytes_read);

    BK_LOGI(TAG, "buffer %s.\n", buffer);

    ret = bk_genie_rsp_parse_update(buffer);
__exit:
    if (session)
    {
        webclient_close(session);
    }

    if (buffer)
    {
        web_free(buffer);
    }

    if (post_data)
    {
        os_free(post_data);
    }

    return ret;
#endif
}

int bk_genie_post_nfc_id(uint8_t *nfc_id)
{
    struct webclient_session *session = NULL;
    char *buffer = NULL, *nfc_post_data = NULL;
    char generate_url[256] = {0};
    int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0, ret = -1;
    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == NULL)
    {
        goto __exit;
    }

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s", bk_get_bk_server_url());
    if ((url_len < 0) || (url_len >= MAX_URL_LEN))
    {
        BK_LOGE(TAG, "URL len overflow\r\n");
        return ret;
    }

    /*Generate data*/
    nfc_post_data = os_malloc(POST_DATA_MAX_SIZE);
    if (nfc_post_data == NULL)
    {
        BK_LOGE(TAG, "no memory for post_data buffer\n");
        goto __exit;
    }
    os_memset(nfc_post_data, 0, POST_DATA_MAX_SIZE);

    data_len = os_snprintf(nfc_post_data, POST_DATA_MAX_SIZE, "{\"channel\":\"%s\",\"agent_type_id\":\"%02x:%02x:%02x:%02x:%02x:%02x:%02x\"}", channel_name_record, 
                    nfc_id[0], nfc_id[1], nfc_id[2], nfc_id[3], nfc_id[4], nfc_id[5] ,nfc_id[6]);
    BK_LOGI(TAG, "%s, %s\r\n", __func__, nfc_post_data);

    webclient_header_fields_add(session, "Content-Length: %d\r\n", os_strlen(nfc_post_data));
    webclient_header_fields_add(session, "Content-Type: application/json\r\n");

    buffer = (char *) web_malloc(RCV_BUF_SIZE);
    if (buffer == NULL)
    {
        BK_LOGE(TAG, "no memory for receive response buffer.\n");
        goto __exit;
    }
    os_memset(buffer, 0, RCV_BUF_SIZE);

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, generate_url, nfc_post_data, data_len)) != 200)
    {
        BK_LOGE(TAG, "webclient POST request failed, response(%d) error.\n", resp_status);
        goto __exit;
    }

    BK_LOGI(TAG, "webclient post response data: \n");
    do
    {
        bytes_read = webclient_read(session, buffer, RCV_BUF_SIZE);
        if (bytes_read > 0)
        {
            break;
        }
    }
    while (1);
    BK_LOGI(TAG, "bytes_read: %d\n", bytes_read);

    BK_LOGI(TAG, "buffer %s.\n", buffer);

__exit:
    if (session)
    {
        webclient_close(session);
    }

    if (buffer)
    {
        web_free(buffer);
    }

    if (nfc_post_data)
    {
        os_free(nfc_post_data);
    }

    return ret;

}

#if CONFIG_ENABLE_AGORA_DATASTREAM
#include "base_64.h"
#define CONFIG_DATASTREAM_TASK_PRIORITY 4
static beken_thread_t datastream_thread_handle = NULL;
beken_queue_t datastream_queue = NULL;
#define MAX_DATASTREAM_SPLIT 4
#define MAX_DATASTREAM_LEN 1024*4
void parse_data_stream_main()
{
    char *save_ptr = NULL, *store_str = NULL,
		*msg_payload = NULL, *decode_str = NULL;
    const char *delim = "|";
    char *msg_id = NULL, *last_msg_id = NULL, *cur_index_str = NULL, *total_num_str = NULL;
    uint8_t cur_index = 0, total_num = 0, store_cur_index = 0, store_total_num = 0;
    int  remaining_len = 0;
    __maybe_unused int ret = 0, decode_len;
    bk_agora_ai_data_stream_t msg;

    while (1) {
        ret = rtos_pop_from_queue(&datastream_queue, &msg, BEKEN_WAIT_FOREVER);
        //message id
        msg_id = strtok_r(msg.data, delim, &save_ptr);
        cur_index_str = strtok_r(NULL, delim, &save_ptr);
	 total_num_str = strtok_r(NULL, delim, &save_ptr);
	 //pkt index
	 cur_index = os_strtoul(cur_index_str, NULL, 10);
	 //total pkt num
	 total_num = os_strtoul(total_num_str, NULL, 10);
	 //message content
	 msg_payload = strtok_r(NULL, delim, &save_ptr);
	 if (!last_msg_id) {
		last_msg_id = os_strdup(msg_id);
	 } else {
		if (os_strcmp(msg_id, last_msg_id)) {
			os_free(last_msg_id);
			last_msg_id = os_strdup(msg_id);
		}
	 }
	 if (!last_msg_id) {
                BK_LOGI(TAG,"OOM!\r\n");
                goto new_msg_loop;
        }
        store_cur_index = cur_index;
        store_total_num = total_num;
        if (store_total_num > MAX_DATASTREAM_SPLIT)
            goto new_msg_loop;
        if (store_cur_index < 1 ||store_cur_index > store_total_num)
            goto new_msg_loop;

        //check decode string
        if (!decode_str)
            decode_str = psram_zalloc(1025*total_num);
        if (store_cur_index == 1 && decode_str) {
            os_free(decode_str);
            decode_str = psram_zalloc(1025*total_num);
        }
        if (!decode_str) {
                BK_LOGI(TAG,"OOM!\r\n");
                goto new_msg_loop;
        }

        //check store string and store
        if (!store_str) {
            store_str = psram_zalloc(MAX_DATASTREAM_LEN+1);
            if (!store_str) {
                BK_LOGI(TAG,"OOM!\r\n");
                goto new_msg_loop;
            }
        }

        remaining_len = MAX_DATASTREAM_LEN - os_strlen(store_str);
        os_snprintf(store_str+os_strlen(store_str), remaining_len, "%s", msg_payload);
        //decode data stream
        if (store_cur_index == store_total_num) {
		BK_LOGI(TAG,"enc_data: %s\r\n", store_str);
		base64_decode((unsigned char *)store_str, os_strlen(store_str), &decode_len, (unsigned char *)decode_str);
		BK_LOGI(TAG,"dec_data: %s\r\n", decode_str);
		//for customer further development
		goto new_msg_loop;
        }
	 os_free(msg.data);
	 continue;
new_msg_loop:
        os_free(msg.data);
        if (last_msg_id) {
            os_free(last_msg_id);
            last_msg_id = NULL;
        }
        if (decode_str) {
            os_free(decode_str);
            decode_str = NULL;
        }
        if (store_str) {
            os_free(store_str);
            store_str = NULL;
        }
    }
}

int bk_genie_init_datastream_resource()
{
    int ret = 0;

    ret = rtos_init_queue(&datastream_queue,
							 "datastream_queue",
							 sizeof(char *),
							 4);

#if CONFIG_PSRAM_AS_SYS_MEMORY
    ret = rtos_create_psram_thread(&datastream_thread_handle,
                                CONFIG_DATASTREAM_TASK_PRIORITY,
                                "parse_data_stream",
                                (beken_thread_function_t)parse_data_stream_main,
                                4096,
                                (beken_thread_arg_t)0);
#else
    ret = rtos_create_thread(&datastream_thread_handle,
                                CONFIG_DATASTREAM_TASK_PRIORITY,
                                "parse_data_stream",
                                (beken_thread_function_t)parse_data_stream_main,
                                4096,
                                (beken_thread_arg_t)0);
#endif

    return ret;
}
#endif

