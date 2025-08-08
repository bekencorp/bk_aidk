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
#include "bk_smart_config_lingxin_adapter.h"
#include "bk_smart_config.h"
#include "app_event.h"
#include "bk_factory_config.h"
#include "wifi_boarding_utils.h"
#include <stdio.h>
#if !CONFIG_ENABLE_LINGXIN_COSTOM_AUTH
#include "driver/trng.h"
#include "auth_config.h"
#endif
#include "chat_state_machine.h"
#define TAG "bk_sconf_lingxin"

#if !CONFIG_ENABLE_LINGXIN_COSTOM_AUTH
#define RCV_BUF_SIZE            256
#define SEND_HEADER_SIZE           1024
#define POST_DATA_MAX_SIZE  1024*2
#define MAX_URL_LEN         256
char *channel_name_record = NULL;
extern struct lingxin_auth_config auth_config;
#endif

void bk_app_notify_wakeup_event(void)
{
    BK_LOGI(TAG, "%s line:%d State_Event_Wakeup_Detected\r\n", __func__, __LINE__);
    state_machine_run_event(State_Event_Wakeup_Detected);
}

extern void beken_auto_run(uint8_t reset);
void bk_sconf_trans_start(void)
{
     beken_auto_run(0);
}

void bk_sconf_trans_stop(void)
{
}

int bk_sconf_post_nfc_id(uint8_t *nfc_id)
{
    return 0;
}

void bk_sconf_begin_to_switch_ir_mode(void)
{
}

#if !CONFIG_ENABLE_LINGXIN_COSTOM_AUTH
int bk_sconf_save_agent_info(char *channel_name)
{
    bk_sconf_agent_info_t info_tmp = {0};

    info_tmp.valid = 1;
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
    len = os_snprintf(payload, max_len, "{\"channel\":\"%s\"}", uid_str);
    BK_LOGI(TAG, "ori channel name:%s, %d\r\n", uid_str, len);
    return len;
}

void  bk_sconf_prase_agent_info(char *payload, uint8_t reset)
{
#if !CONFIG_ENABLE_LINGXIN_COSTOM_AUTH
    cJSON *json = NULL;
    int ret __maybe_unused = 0;

    json = cJSON_Parse(payload);
    if (!json)
    {
        BK_LOGE(TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
        return;
    }
    if (channel_name_record)
    {
        os_free(channel_name_record);
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
    if (channel_name_record)
    {
        bk_sconf_save_agent_info(channel_name_record);
        BK_LOGI(TAG, "begin agora_auto_run\n");

        if (!bk_sconf_is_net_pan_configured())
        {
            app_event_send_msg(APP_EVT_CLOSE_BLUETOOTH, 0);
        }

        bk_sconf_sync_flash();
        beken_auto_run(reset);
    }
#else
    BK_LOGI(TAG, "begin agora_auto_run\n");

    if (!bk_sconf_is_net_pan_configured())
    {
        app_event_send_msg(APP_EVT_CLOSE_BLUETOOTH, 0);
    }

    bk_sconf_sync_flash();
    beken_auto_run(reset);
#endif
}

#if !CONFIG_ENABLE_LINGXIN_COSTOM_AUTH
int bk_sconf_rsp_parse_update(char *buffer)
{
    __maybe_unused bk_sconf_agent_info_t info = {0};
    cJSON *json = cJSON_Parse(buffer);
    if (!json)
    {
        BK_LOGE(TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
        return BK_FAIL;
    }

    cJSON* data = cJSON_GetObjectItem(json, "data");
    if (data == NULL) {
        cJSON_Delete(json);
        BK_LOGE(TAG, "Not found data object.");
        return -1;
    }
    
    cJSON* sn_item = cJSON_GetObjectItem(data, "sn");
    const char* sn = cJSON_GetStringValue(sn_item);
    if (auth_config.sn)
    {
        os_free(auth_config.sn);
        auth_config.sn = NULL;
    }
    auth_config.sn = os_strdup(sn);
    
    cJSON* appid_item = cJSON_GetObjectItem(data, "app_id");
    const char* appid = cJSON_GetStringValue(appid_item);
    if (auth_config.appId)
    {
        os_free(auth_config.appId);
        auth_config.appId = NULL;
    }
    auth_config.appId = os_strdup(appid);
    
    cJSON* appkey_item = cJSON_GetObjectItem(data, "app_key");
    const char* appkey = cJSON_GetStringValue(appkey_item);
    if (auth_config.appKey)
    {
        os_free(auth_config.appKey);
        auth_config.appKey = NULL;
    }
    auth_config.appKey = os_strdup(appkey);
    
    cJSON* agentcode_item = cJSON_GetObjectItem(data, "agent_code");
    const char* agentcode = cJSON_GetStringValue(agentcode_item);
    if (auth_config.agentCode)
    {
        os_free(auth_config.agentCode);
        auth_config.agentCode = NULL;
    }
    auth_config.agentCode = os_strdup(agentcode);
    
    cJSON_Delete(json);

    return BK_OK;
}

extern char *bk_get_bk_server_url(uint8_t index);
int bk_sconf_wakeup_agent(uint8_t reset)
{
    struct webclient_session *session = NULL;
    char *buffer = NULL, *post_data = NULL;
    char generate_url[256] = {0};
    int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0, ret = 0;
    uint32_t rand_flag = 0;
    __maybe_unused bk_sconf_agent_info_t info = {0};

    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == NULL)
    {
        ret = -1;
        goto __exit;
    }

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s/activate_agent/", bk_get_bk_server_url(2));
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

    if (bk_sconf_get_agent_info(&info) == 0)
    {
        if (info.valid != 1)
        {
            return -1;
        }
        if (channel_name_record)
        {
            os_free(channel_name_record);
        }
        channel_name_record = os_strdup(info.channel_name);
        if (channel_name_record)
            BK_LOGI(TAG, "%s\r\n", channel_name_record);
        else
            return -1;
    }

    data_len = os_snprintf(post_data, POST_DATA_MAX_SIZE, "{\"channel\":\"%s\",", channel_name_record);
    //agent_param reserved for further development, beken server solution
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"agent_param\": {");
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"voice_in_format\": \"pcm\",\"voice_out_format\": \"mp3\",");
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"voice_in_sample_rate\": 16000,\"voice_out_sample_rate\": 16000");
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
}
#endif