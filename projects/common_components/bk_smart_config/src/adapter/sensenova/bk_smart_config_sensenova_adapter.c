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
#include "bk_websocket_client.h"
#include "bk_smart_config_sensenova_adapter.h"

#define TAG "bk_sconf_agora"
#define RCV_BUF_SIZE            256
#define SEND_HEADER_SIZE           1024
#define POST_DATA_MAX_SIZE  1024*2
#define MAX_URL_LEN         256


char *app_id_record = NULL;
char *channel_name_record = NULL;
char *token_record = NULL;
uint32_t uid_record = 0;

extern void agora_auto_run(uint8_t reset);

void bk_app_notify_wakeup_event(void)
{
    //need to be implemented if necessary
}

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
    os_printf("bk_sconf_rsp_parse_update buffer:%s\n", buffer);
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
    os_printf("data:%s\n", data->valuestring);
	if (data)
	{
		cJSON *token = cJSON_GetObjectItem(data, "token");
		if (token && ((token->type & 0xFF) == cJSON_String)) {
			token_record = os_strdup(token->valuestring);
			BK_LOGI(TAG, "[UPDATE] token:%s size:%d\n", token_record, strlen(token_record));
		}
		else {
			BK_LOGE(TAG, "[Error] not find token msg\n");
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
transport sensenova_wss = NULL;
static beken_semaphore_t sensenova_wss_sem = NULL;
int bk_sconf_sensenova_wss_send_text(transport web_socket, enum sensenova_wss_type msgtype) {
    int ret = 0;
	if (web_socket == NULL) {
		BK_LOGE(TAG, "Invalid arguments\r\n");
		return -1;
	}

    char *buf = NULL;
	if (NULL == (buf = (char *)os_zalloc(SENSENOVA_WSS_TXT_SIZE))) {
		BK_LOGE(TAG, "alloc user context fail\r\n");
		return -1;
	}

    int n = 0;
	switch (msgtype) {
        case CREATE_SESSION:
            n = snprintf(buf, SENSENOVA_WSS_TXT_SIZE, "{\"type\":\"CreateSession\",\"request_id\":\"a5d62e7df1d14db5ae5aed7eda9499b3\"}");
            ret = websocket_client_send_text(web_socket, buf, n, 10*1000);
            break;
        case REQUEST_AGORA_CHANNEL_INFO:
            n = snprintf(buf, SENSENOVA_WSS_TXT_SIZE, "{\"type\":\"RequestAgoraChannelInfo\",\"request_id\":\"a5d62e7df1d14db5ae5aed7eda9499b3\"}");
            ret = websocket_client_send_text(web_socket, buf, n, 10*1000);
            break;
        case REQUEST_AGORA_TOKEN:
            n = snprintf(buf, SENSENOVA_WSS_TXT_SIZE, "{\"type\":\"RequestAgoraToken\",\"duration\":6000,\"request_id\":\"a5d62e7df1d14db5ae5aed7eda9499b3\"}");
            ret = websocket_client_send_text(web_socket, buf, n, 10*1000);
            break;
        case START_SERVING:
            n = snprintf(buf, SENSENOVA_WSS_TXT_SIZE, "{\"type\":\"StartServing\"}");
            ret = websocket_client_send_text(web_socket, buf, n, 10*1000);
            break;
        default:
            BK_LOGI(TAG, "Unsupported message type");
            break;
    }

    if(ret < 0)
    {
        BK_LOGE(TAG, "sensenova_wss_send_text fail buf:%s ret:%d\r\n", buf, ret);
    }

    if(buf)
        os_free(buf);
    return n;  // Returns the number of bytes sent
}
void bk_sconf_sensenova_wss_txt_handle(char *json_text, unsigned int size)
{
    //json_text[size] = '\0';
    //BK_LOGI(TAG, "sensenova_wss RXtxt:%s\r\n", json_text);
    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL) {
        BK_LOGE(TAG, "Error: Failed to parse JSON text:%s\n", json_text);
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type == NULL) {
        BK_LOGE(TAG, "Error: Missing type field.\n");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(type->valuestring, "CreateSessionResult") == 0) {
        BK_LOGI(TAG, "RX CreateSessionResult\r\n");
        bk_sconf_sensenova_wss_send_text(sensenova_wss, REQUEST_AGORA_CHANNEL_INFO);
    }
    else if (strcmp(type->valuestring, "AgoraChannelInfo") == 0) {
        BK_LOGI(TAG, "RX AgoraChannelInfo\r\n");
        bk_sconf_sensenova_wss_send_text(sensenova_wss, REQUEST_AGORA_TOKEN);
        cJSON *app_id = cJSON_GetObjectItem(root, "appid");
        if(app_id)
        {
            BK_LOGI(TAG, "appid:%s\r\n", app_id->valuestring);
            if(app_id_record)
            {
                os_free(app_id_record);
            }
            app_id_record = os_strdup(app_id->valuestring);
        }

        cJSON *chn_id = cJSON_GetObjectItem(root, "channel_id");
        if(chn_id)
        {
            BK_LOGI(TAG, "channel_id:%s\r\n", chn_id->valuestring);
            if(channel_name_record)
            {
                os_free(channel_name_record);
            }
            channel_name_record = os_strdup(chn_id->valuestring);
        }
    } else if (strcmp(type->valuestring, "AgoraToken") == 0) {
        BK_LOGI(TAG, "RX AgoraToken\r\n");
        cJSON *token = cJSON_GetObjectItem(root, "token");
        if(token)
        {
            BK_LOGI(TAG, "token:%s\r\n", token->valuestring);
            if(token_record)
            {
                os_free(token_record);
            }
            token_record = os_strdup(token->valuestring);
        }
        cJSON *uid = cJSON_GetObjectItem(root, "client_uid");
        if(uid)
        {
            BK_LOGI(TAG, "client_uid:%d\r\n", uid->valueint);
            uid_record = uid->valueint;
        }
        bk_sconf_sensenova_wss_send_text(sensenova_wss, START_SERVING);
        if (sensenova_wss_sem)
            rtos_set_semaphore(&sensenova_wss_sem);
    }
    else {
        //BK_LOGE(TAG, "Warning: Unknown type: %s\n", type->valuestring);
    }
    cJSON_Delete(root);
}

void bk_sconf_websocket_event_handler(void* event_handler_arg, char *event_base, int32_t event_id, void* event_data)
{
    bk_websocket_event_data_t *data = (bk_websocket_event_data_t *)event_data;
    transport client = (transport)event_handler_arg;
    //BK_LOGI(TAG, "event_id:%d data->op_code:%d\r\n", event_id, data->op_code);

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            BK_LOGI(TAG, "Connected to WebSocket server\r\n");
            bk_sconf_sensenova_wss_send_text(client, CREATE_SESSION);
            break;
        case WEBSOCKET_EVENT_CLOSED:
            BK_LOGI(TAG, "WEBSOCKET_EVENT_CLOSED\r\n");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            BK_LOGI(TAG, "Disconnected from WebSocket server\r\n");
            break;
        case WEBSOCKET_EVENT_DATA: {
            //BK_LOGI(TAG, "data from WebSocket server, len:%d op:%d\r\n", data->data_len, data->op_code);
            if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
                bk_sconf_sensenova_wss_txt_handle(data->data_ptr, data->data_len);
            }
            break;
        }
        default:
            break;
    }
}

/**
 * Wake up the Sensenova agent service and establish WebSocket connection
 * @param reset: Reset flag (not currently used in implementation)
 * @return BK_OK on success, BK_FAIL on failure
 */
int bk_sconf_wakeup_agent(uint8_t reset)
{
    //int ret = 0;
    websocket_client_input_t websocket_cfg = {0};  // WebSocket client configuration
    struct webclient_session *session = NULL;      // Web client session handle
    char *buffer = NULL, *post_data = NULL;        // Buffers for response and post data
    char *wss_uri = NULL;                          // WebSocket URI
    char generate_url[256] = {0};                  // URL for activation request
    int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0, ret = 0;  // Various status variables

    // Check if channel name is available
    if (channel_name_record == NULL)
    {
        ret = -1;
        BK_LOGE(TAG, "channel_name_record is NULL\r\n");
        goto __exit;
    }
    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == NULL)
    {
        ret = -1;
        goto __exit;
    }

    // Generate activation URL
    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s/activate_agent/", bk_get_bk_server_url(3));
    if ((url_len < 0) || (url_len >= MAX_URL_LEN))
    {
        BK_LOGE(TAG, "URL len overflow\r\n");
        ret = -1;
        return ret;
    }

    /*Generate data for POST request*/
    post_data = os_malloc(POST_DATA_MAX_SIZE);
    if (post_data == NULL)
    {
        ret = -1;
        BK_LOGE(TAG, "no memory for post_data buffer\n");
        goto __exit;
    }
    os_memset(post_data, 0, POST_DATA_MAX_SIZE);

    // Prepare JSON payload with authentication info if available
    if (strlen(CONFIG_SENSENOVA_ISS) && strlen(CONFIG_SENSENOVA_SECRET))
    {
        data_len = os_snprintf(post_data, POST_DATA_MAX_SIZE, "{\"channel\":\"%s\",\"iss\":\"%s\",\"secret\":\"%s\"}", channel_name_record, CONFIG_SENSENOVA_ISS, CONFIG_SENSENOVA_SECRET);
    }
    else
    {
        data_len = os_snprintf(post_data, POST_DATA_MAX_SIZE, "{\"channel\":\"%s\"}", channel_name_record);
    }

    BK_LOGI(TAG, "%s, %s\r\n", __func__, post_data);

    // Add necessary headers for POST request
    webclient_header_fields_add(session, "Content-Length: %d\r\n", os_strlen(post_data));
    webclient_header_fields_add(session, "Content-Type: application/json\r\n");

    // Allocate buffer for response
    buffer = (char *) web_malloc(RCV_BUF_SIZE);
    if (buffer == NULL)
    {
        ret = -1;
        BK_LOGE(TAG, "no memory for receive response buffer.\n");
        goto __exit;
    }
    os_memset(buffer, 0, RCV_BUF_SIZE);

    /* send POST request by default header to get JWT token */
    if ((resp_status = webclient_post(session, generate_url, post_data, data_len)) != 200)
    {
        ret = -1;
        BK_LOGE(TAG, "webclient POST request failed, response(%d) error.\n", resp_status);
        goto __exit;
    }

    BK_LOGI(TAG, "webclient post response data: \n");
    // Read response data
    do
    {
        bytes_read = webclient_read(session, buffer, RCV_BUF_SIZE);
        if (bytes_read > 0)
        {
            break;
        }
    }
    while (1);
    BK_LOGI(TAG, "bytes_read: %d buf:%s\n", bytes_read, buffer);

    // get JWT token from server response
    ret = bk_sconf_rsp_parse_update(buffer);
    if (ret != BK_OK)
    {
        BK_LOGE(TAG, "bk_sconf_rsp_parse_update fail\r\n");
        goto __exit;
    }

    // Prepare WebSocket URI with token
    wss_uri = os_malloc(400);
    if (wss_uri && token_record)
    {
        sprintf(wss_uri, "%s?%s&jwt=%s", CONFIG_SENSENOVA_WSS_URL, CONFIG_SENSENOVA_LLM_CONFIG, token_record);
        websocket_cfg.uri = wss_uri;
    }
    else
    {
        BK_LOGE(TAG, "token_record is NULL\r\n");
        ret = -1;
        goto __exit;
    }

    // Configure WebSocket event handler
    websocket_cfg.ws_event_handler = bk_sconf_websocket_event_handler;
    //websocket_cfg.ws_event_handler_arg = (void *)sensenova_wss;
    //websocket_cfg.authorization = "Bearer";
    // Initialize WebSocket client
    sensenova_wss = websocket_client_init(&websocket_cfg);
    if (!sensenova_wss)
    {
        BK_LOGE(TAG, "%s fail\r\n", __func__);
        return -1;
    }

    // Start WebSocket client
    if(websocket_client_start(sensenova_wss)) {
        BK_LOGE(TAG, "%s fail\r\n", __func__);
        goto __exit;
    }

    // Initialize semaphore for WebSocket synchronization
    ret = rtos_init_semaphore(&sensenova_wss_sem, 1);
    if (ret != BK_OK)
    {
        BK_LOGE(TAG, "%s, %d, create semaphore fail\n", __func__, __LINE__);
        goto __exit;
    }

    // Wait for semaphore with timeout
    ret = rtos_get_semaphore(&sensenova_wss_sem, 5000);
    if (ret != BK_OK)
    {
        BK_LOGE(TAG, "%s, %d, get semaphore timeout\n", __func__, __LINE__);
        goto __exit;
    }

    return BK_OK;
__exit:
    // Cleanup resources
    if (wss_uri)
    {
        os_free(wss_uri);
        wss_uri = NULL;
    }

    if (sensenova_wss_sem)
    {
        rtos_deinit_semaphore(&sensenova_wss_sem);
        sensenova_wss_sem = NULL;
    }

    if (sensenova_wss)
    {
        websocket_client_destroy(sensenova_wss);
        sensenova_wss = NULL;
    }

    return BK_FAIL;
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
    os_printf("bk_sconf_prase_agent_info payload:%s\n", payload);
    json = cJSON_Parse(payload);
    if (!json)
    {
        BK_LOGE(TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
        return;
    }

    cJSON *app_id = cJSON_GetObjectItem(json, "app_id");
    if (app_id && ((app_id->type & 0xFF) == cJSON_String))
    {
        app_id_record = os_strdup(app_id->valuestring);
    }
    else
    {
        app_id_record = os_strdup("init_app_id");
    }

    cJSON *channel_name = cJSON_GetObjectItem(json, "channel_name");
    if (channel_name && ((channel_name->type & 0xFF) == cJSON_String))
    {
        channel_name_record = os_strdup(channel_name->valuestring);
        BK_LOGI(TAG, "real channel name:%s\r\n", channel_name_record);
    }
    else
    {
        BK_LOGE(TAG, "[Error] not find channel_name msg\n");
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

    BK_LOGW(TAG, "Do not support image recognition mode switch!\r\n");
        goto exit;

    if (!agora_runing) {
        BK_LOGW(TAG, "Please Run AgoraRTC First!\r\n");
        goto exit;
    }

//     ir_mode_switching = 1;

// #if CONFIG_BT && (CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO) && CONFIG_BT_REUSE_MEDIA_MEMORY
//     BK_LOGW(TAG, "you can't enable bluetooth and video at sametime when CONFIG_BT_REUSE_MEDIA_MEMORY=y !!!\n");
// #else
//     if (!video_started)
//     {
//         bk_sconf_upate_agent_info("text_and_image");
//         while (g_agent_offline)
//         {
//             if (!agora_runing)
//             {
//                 goto exit;
//             }
//             rtos_delay_milliseconds(100);
//             ir_timeout++;
//             // 2s timeout, fail
//             if (ir_timeout >= 20) {
//                 BK_LOGW(TAG, "Image recognition mode switch timeout\r\n");
//                 goto exit;
//             }
//         }
//         video_turn_on();

// #if (CONFIG_DUAL_SCREEN_AVI_PLAY)
//         if (lvgl_app_init_flag == 1) {
//             media_app_lvgl_switch_ui(LVGL_UI_DISP_IN_TEXT_AND_IMAGE);
//         }
// #endif
//     }
//     else
// #endif
//     {
//         video_turn_off();
//         bk_sconf_upate_agent_info("text");

// #if (CONFIG_DUAL_SCREEN_AVI_PLAY)
//         if (lvgl_app_init_flag == 1) {
//             media_app_lvgl_switch_ui(LVGL_UI_DISP_IN_TEXT);
//         }
// #endif
//     }

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
    //channel_name_record = os_strdup("init_channel_name");
    //app_id_record = os_strdup("init_app_id");
    BK_LOGI(TAG, "%s %d first_time_for_network_reconnect:%d\r\n", __func__, __LINE__, first_time_for_network_reconnect);
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
    if (sensenova_wss) {
        websocket_client_destroy(sensenova_wss);
        sensenova_wss = NULL;
    }
}
