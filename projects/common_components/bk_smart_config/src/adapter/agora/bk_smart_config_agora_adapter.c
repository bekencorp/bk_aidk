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
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include "components/webclient.h"
#include "cJSON.h"
#include "components/bk_uid.h"
#include "bk_genie_comm.h"
#include "bk_smart_config_agora_adapter.h"
#include "bk_smart_config.h"
#include "bk_factory_config.h"
#include "driver/trng.h"
#include "app_event.h"
#include "media_app.h"
#include "agora_config.h"
#include "video_engine.h"
#include "wifi_boarding_utils.h"

#define TAG "bk_sconf_agora"
#define RCV_BUF_SIZE            256
#define SEND_HEADER_SIZE           1024
#define POST_DATA_MAX_SIZE  1024*2
#define MAX_URL_LEN         256
char *app_id_record = NULL;
char *channel_name_record = NULL;
char *token_record = NULL;
uint32_t uid_record = 0;

#if  CONFIG_BK_DEV_STARTUP_AGENT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <os/mem.h>
#include <os/str.h>
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
#if CONFIG_AUD_INTF_SUPPORT_OPUS
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
void bk_app_notify_wakeup_event(void)
{
    //need to be implemented if necessary
}

extern void agora_auto_run(uint8_t reset);
static void bk_sconf_start_agora_rtc(uint8_t reset)
{
    __maybe_unused bk_sconf_agent_info_t info = {0};
#if CONFIG_BK_DEV_STARTUP_AGENT
    agora_auto_run(reset);
#else
    if (bk_sconf_get_agent_info(&info) == 0)
    {
        if (info.valid != 1)
        {
            return;
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
        agora_auto_run(reset);
    }
#endif
}

void bk_sconf_erase_agent_info(void)
{
    bk_sconf_agent_info_t info_tmp = {0};

    bk_config_write("d_agent_info", (const void *)&info_tmp, sizeof(bk_sconf_agent_info_t));
}

int bk_sconf_save_agent_info(char *appid, char *channel_name)
{
    bk_sconf_agent_info_t info_tmp = {0};

    info_tmp.valid = 1;
    os_strcpy(info_tmp.appid, appid);
    os_strcpy(info_tmp.channel_name, channel_name);
    bk_config_write("d_agent_info", (const void *)&info_tmp, sizeof(bk_sconf_agent_info_t));

    return 0;
}

int bk_sconf_get_agent_info(bk_sconf_agent_info_t *info)
{
    bk_sconf_agent_info_t info_tmp = {0};

    if (bk_config_read("d_agent_info", (void *)&info_tmp, sizeof(bk_sconf_agent_info_t)) <= 0)
    {
        return -1;
    }
    os_memcpy(info, &info_tmp, sizeof(bk_sconf_agent_info_t));
    return 0;
}

int bk_sconf_rsp_parse_update(char *buffer)
{
	char *app_id_update = NULL;
	int code_update = 0;
	__maybe_unused bk_sconf_agent_info_t info = {0};
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

	if (bk_sconf_get_agent_info(&info) == 0)
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
		bk_sconf_save_agent_info(app_id_record, channel_name_record);
	}
	if (app_id_update)
		os_free(app_id_update);
	return BK_OK;
}

extern char *bk_get_bk_server_url(uint8_t index);
int bk_sconf_wakeup_agent(uint8_t reset)
{
#if CONFIG_BK_DEV_STARTUP_AGENT
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

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s/activate_agent/", bk_get_bk_server_url(0));
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
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"reset\":%u,\"agent_param\": {", reset);
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"audio_duration\": %d,", CONFIG_AUDIO_FRAME_DURATION_MS);
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"out_acodec\": \"%s\"",CONFIG_AUDIO_ENCODER_TYPE);
    rand_flag = bk_rand();
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "},\"rand_flag\":\"%u\"}", rand_flag);
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

    ret = bk_sconf_rsp_parse_update(buffer);
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

int bk_sconf_upate_agent_info(char *update_info)
{
    struct webclient_session *session = NULL;
    char *buffer = NULL, *post_data = NULL;
    char generate_url[256] = {0};
    int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0, ret = 0;

    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == NULL)
    {
        ret = -1;
        goto __exit;
    }

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s/switch_model_type/", bk_get_bk_server_url(0));
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
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"model_type\":\"%s\"}", update_info);
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

    ret = bk_sconf_rsp_parse_update(buffer);
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
}

int bk_sconf_post_nfc_id(uint8_t *nfc_id)
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

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s/activate_agent/", bk_get_bk_server_url(0));
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

int bk_sconf_init_datastream_resource()
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

uint16_t bk_sconf_send_agent_info(char *payload, uint16_t max_len)
{
    unsigned char uid[32] = {0};
    char uid_str[65] = {0};
    uint16 len = 0;

    bk_uid_get_data(uid);
    for (int i = 0; i < 24; i++)
    {
        sprintf(uid_str + i * 2, "%02x", uid[i]);
    }
    len = os_snprintf(payload, max_len, "{\"channel\":\"%s\",\"agent_param\": {", uid_str);
    len += os_snprintf(payload+len, max_len, "\"audio_duration\": %d,", CONFIG_AUDIO_FRAME_DURATION_MS);
    len += os_snprintf(payload+len, max_len, "\"out_acodec\": \"%s\"",CONFIG_AUDIO_ENCODER_TYPE);
    len += os_snprintf(payload+len, max_len, "}}");
    BK_LOGI(TAG, "ori channel name:%s, %s, %d\r\n", uid_str, payload, len);
    return len;
}

void  bk_sconf_prase_agent_info(char *payload, uint8_t reset)
{
#if !CONFIG_BK_DEV_STARTUP_AGENT
    cJSON *json = NULL;
    int ret __maybe_unused = 0;

    json = cJSON_Parse(payload);
    if (!json)
    {
        BK_LOGE(TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
        return;
    }
    if (app_id_record)
    {
        os_free(app_id_record);
    }
    if (channel_name_record)
    {
        os_free(channel_name_record);
    }
    cJSON *app_id = cJSON_GetObjectItem(json, "app_id");
    if (app_id && ((app_id->type & 0xFF) == cJSON_String))
    {
        app_id_record = os_strdup(app_id->valuestring);
    }
    else
    {
        BK_LOGE(TAG, "[Error] not find msg\n");
    }

    cJSON *channel_name = cJSON_GetObjectItem(json, "channel_name");
    if (channel_name && ((channel_name->type & 0xFF) == cJSON_String))
    {
        channel_name_record = os_strdup(channel_name->valuestring);
        BK_LOGI(TAG, "real channel name:%s\r\n", channel_name_record);
    }
    else
    {
        BK_LOGE(TAG, "[Error] not find msg\n");
    }
    cJSON_Delete(json);
    if (app_id_record && channel_name_record)
    {
        bk_sconf_save_agent_info(app_id_record, channel_name_record);
        BK_LOGI(TAG, "begin agora_auto_run\n");

        if (!bk_sconf_is_net_pan_configured())
        {
            app_event_send_msg(APP_EVT_CLOSE_BLUETOOTH, 0);
        }

        bk_sconf_sync_flash();
        agora_auto_run(reset);
    }
#else
    if (!bk_sconf_is_net_pan_configured())
    {
        app_event_send_msg(APP_EVT_CLOSE_BLUETOOTH, 0);
    }

    bk_sconf_sync_flash();
    agora_auto_run(reset);

#endif
}

//image recognition mode switch
uint8_t ir_mode_switching = 0;
#if !CONFIG_BK_DEV_STARTUP_AGENT
#define CONFIG_IR_MODE_SWITCH_TASK_PRIORITY 4
static beken_thread_t config_ir_mode_switch_thread_handle = NULL;
extern bool agora_runing;
extern bool g_agent_offline;
extern bool video_started;
extern bk_err_t video_turn_on(void);
extern bk_err_t video_turn_off(void);
#if (CONFIG_DUAL_SCREEN_AVI_PLAY)
extern uint8_t lvgl_app_init_flag;
#endif
void ir_mode_switch_main(void)
{
    __maybe_unused uint8_t ir_timeout = 0;

    if (!agora_runing) {
        BK_LOGW(TAG, "Please Run AgoraRTC First!\r\n");
        goto exit;
    }

    ir_mode_switching = 1;

#if CONFIG_BT && (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO) && CONFIG_BT_REUSE_MEDIA_MEMORY
    BK_LOGW(TAG, "you can't enable bluetooth and video at sametime when CONFIG_BT_REUSE_MEDIA_MEMORY=y !!!\n");
#else
    if (!video_started)
    {
        bk_sconf_upate_agent_info("text_and_image");
        while (g_agent_offline)
        {
            if (!agora_runing)
            {
                goto exit;
            }
            rtos_delay_milliseconds(100);
            ir_timeout++;
            // 2s timeout, fail
            if (ir_timeout >= 20) {
                BK_LOGW(TAG, "Image recognition mode switch timeout\r\n");
                goto exit;
            }
        }
        video_turn_on();

#if (CONFIG_DUAL_SCREEN_AVI_PLAY)
        if (lvgl_app_init_flag == 1) {
            media_app_lvgl_switch_ui(LVGL_UI_DISP_IN_TEXT_AND_IMAGE);
        }
#endif
    }
    else
#endif
    {
        video_turn_off();
        bk_sconf_upate_agent_info("text");

#if (CONFIG_DUAL_SCREEN_AVI_PLAY)
        if (lvgl_app_init_flag == 1) {
            media_app_lvgl_switch_ui(LVGL_UI_DISP_IN_TEXT);
        }
#endif
    }

exit:
    config_ir_mode_switch_thread_handle = NULL;
    ir_mode_switching = 0;
    rtos_delete_thread(NULL);
}

void bk_sconf_begin_to_switch_ir_mode(void)
{
    int ret = 0;

    if (config_ir_mode_switch_thread_handle) {
        BK_LOGW(TAG, "Last oper for IR_MODE ongoing!\n");
        return;
    }
    BK_LOGW(TAG, "Start to switch image recognition mode!\n");
#if CONFIG_PSRAM_AS_SYS_MEMORY
    ret = rtos_create_psram_thread(&config_ir_mode_switch_thread_handle,
                                CONFIG_IR_MODE_SWITCH_TASK_PRIORITY,
                                "ir_mode_switch",
                                (beken_thread_function_t)ir_mode_switch_main,
                                4096,
                                (beken_thread_arg_t)0);
#else
    ret = rtos_create_thread(&config_ir_mode_switch_thread_handle,
                                CONFIG_IR_MODE_SWITCH_TASK_PRIORITY,
                                "ir_mode_switch",
                                (beken_thread_function_t)ir_mode_switch_main,
                                4096,
                                (beken_thread_arg_t)0);
#endif
    if (ret != kNoErr)
    {
        BK_LOGE(TAG, "switch image recognition mode fail: %d\r\n", ret);
        config_ir_mode_switch_thread_handle = NULL;
    }
}
#else
void bk_sconf_begin_to_switch_ir_mode(void)
{
    //need to be implemented
}
#endif

extern bool first_time_for_network_reconnect;
void bk_sconf_trans_start(void)
{
    if (first_time_for_network_reconnect) {
        first_time_for_network_reconnect = false;
        bk_sconf_start_agora_rtc(1);
    } else
        bk_sconf_start_agora_rtc(0);
}

extern bk_err_t agora_stop(void);
void bk_sconf_trans_stop(void)
{
    video_turn_off();
    agora_stop();
}
