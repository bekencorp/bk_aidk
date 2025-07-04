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
#include <os/mem.h>
#include "components/webclient.h"
#include "cJSON.h"
#include "components/bk_uid.h"
#include "bk_genie_comm.h"
#include "bk_smart_config_volc_adapter.h"
#include "bk_smart_config.h"
#include "bk_factory_config.h"
#include "driver/trng.h"
#include "app_event.h"
#include "media_app.h"
#include "volc_config.h"
#include "volc_rtc.h"
#include "wifi_boarding_utils.h"

#define TAG "bk_sconf_volc"
#define RCV_BUF_SIZE            512
#define SEND_HEADER_SIZE           1024
#define POST_DATA_MAX_SIZE  1024*2
#define MAX_URL_LEN         256

rtc_room_info_t *volc_room_info = NULL;

#if  CONFIG_BK_DEV_STARTUP_AGENT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <os/mem.h>
#include <os/str.h>
#include "components/webclient.h"
#include "cJSON.h"
#include "RtcBotUtils.h"

int bk_volc_dev_stop_agent(rtc_room_info_t *room_info)
{
    if (!room_info) {
	BK_LOGE(TAG, "stop agent fail, invalid room info\r\n");
	return BK_FAIL;
    }
    stop_voice_bot(room_info);

    return BK_OK;
}

int bk_volc_dev_start_agent(rtc_room_info_t *room_info)
{
    int start_ret = BK_FAIL;

    start_ret = start_voice_bot(room_info);
    if (start_ret != 200) {
        BK_LOGE(TAG, "start agent fail, ret = %d\r\n", start_ret);
        return BK_FAIL;
    }

    return BK_OK;
}

#endif

extern void byte_auto_run(uint8_t reset);
static void bk_sconf_start_volc_rtc(uint8_t reset)
{
    byte_auto_run(reset);
}

void bk_sconf_erase_agent_info(void)
{
    bk_sconf_agent_info_t info_tmp = {0};

    bk_config_write("d_agent_info", (const void *)&info_tmp, sizeof(bk_sconf_agent_info_t));
}

int bk_sconf_save_agent_info(rtc_room_info_t *room_info)
{
    bk_sconf_agent_info_t info_tmp = {0};

    bk_config_read("d_agent_info", (void *)&info_tmp, sizeof(bk_sconf_agent_info_t));
    info_tmp.valid = 1;
    os_memcpy((void *)(&info_tmp.room_info), (const void *)room_info, sizeof(rtc_room_info_t));
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

int bk_sconf_save_channel_name(char *chan)
{
    bk_sconf_agent_info_t info_tmp = {0};

    bk_config_read("d_agent_info", (void *)&info_tmp, sizeof(bk_sconf_agent_info_t));
    os_memset(info_tmp.channel_name, 0x0, 128);
    os_strcpy(info_tmp.channel_name, chan);
    bk_config_write("d_agent_info", (const void *)&info_tmp, sizeof(bk_sconf_agent_info_t));

    return 0;
}

int bk_sconf_get_channel_name(char *chan)
{
    bk_sconf_agent_info_t info_tmp = {0};

    if (bk_config_read("d_agent_info", (void *)&info_tmp, sizeof(bk_sconf_agent_info_t)) <= 0)
    {
        return -1;
    }
    os_strcpy(chan, info_tmp.channel_name);
    return 0;
}

int bk_sconf_rsp_parse_update(char *buffer)
{
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

    cJSON* data = cJSON_GetObjectItem(json, "data");
    
    if (data == NULL) {
        cJSON_Delete(json);
        BK_LOGE(TAG, "Not found data object.");
        return -1;
    }
    
    cJSON* app_id_item = cJSON_GetObjectItem(data, "app_id");
    const char* app_id = cJSON_GetStringValue(app_id_item);
    os_strcpy(volc_room_info->app_id, app_id);
    
    cJSON* uid_item = cJSON_GetObjectItem(data, "uid");
    const char* uid = cJSON_GetStringValue(uid_item);
    os_strcpy(volc_room_info->uid, uid);
    
    cJSON* room_id_item = cJSON_GetObjectItem(data, "room_id");
    const char* room_id = cJSON_GetStringValue(room_id_item);
    os_strcpy(volc_room_info->room_id, room_id);
    
    cJSON* token_item = cJSON_GetObjectItem(data, "token");
    const char* token = cJSON_GetStringValue(token_item);
    os_strcpy(volc_room_info->token, token);
    cJSON_Delete(json);

    bk_sconf_save_agent_info(volc_room_info);

    return BK_OK;
}

extern char *bk_get_bk_server_url(uint8_t index);
int bk_sconf_wakeup_agent(uint8_t reset)
{
#if CONFIG_BK_DEV_STARTUP_AGENT
    rtc_room_info_t room_info = {0};

#if CONFIG_VOLC_HTTP_STARTUP_AGENT
    bk_sconf_agent_info_t info = {0};
    bk_sconf_get_agent_info(&info);
    if (info.valid == 1)
        bk_volc_dev_stop_agent(&info.room_info);

    int ret = BK_FAIL;
    ret = bk_volc_dev_start_agent(&room_info);
    if (ret)
        return ret;
#else
    os_strcpy((char *)room_info.room_id, DEFAULT_ROOM_ID);
    os_strcpy((char *)room_info.uid, DEFAULT_USER_ID);
    os_strcpy((char *)room_info.app_id, DEFAULT_RTC_APP_ID);
    os_strcpy((char *)room_info.token, DEFAULT_TOKEN);
#endif

    if (!volc_room_info)
        volc_room_info = psram_malloc(sizeof(rtc_room_info_t)+1);
    os_memset(volc_room_info, 0, sizeof(rtc_room_info_t)+1);
    os_memcpy(volc_room_info, &room_info, sizeof(rtc_room_info_t));
    bk_sconf_save_agent_info(volc_room_info);
    
    return BK_OK;
#else
    struct webclient_session *session = NULL;
    char *buffer = NULL, *post_data = NULL;
    char generate_url[256] = {0};
    int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0, ret = 0;
    uint32_t rand_flag = 0;
    char tmp_channel[128] = {0};

    if (!volc_room_info)
        volc_room_info = psram_malloc(sizeof(rtc_room_info_t)+1);
    os_memset(volc_room_info, 0, sizeof(rtc_room_info_t)+1);

    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == NULL)
    {
        ret = -1;
        goto __exit;
    }

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s/activate_agent/", bk_get_bk_server_url(1));
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
    bk_sconf_get_channel_name(tmp_channel);
    data_len = os_snprintf(post_data, POST_DATA_MAX_SIZE, "{\"channel\":\"%s\",", tmp_channel);
    //agent_param reserved for further development
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"agent_param\": {");
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
    uint32_t rand_flag = 0;
    char tmp_channel[128] = {0};

    if (!volc_room_info)
        volc_room_info = psram_malloc(sizeof(rtc_room_info_t)+1);
    os_memset(volc_room_info, 0, sizeof(rtc_room_info_t)+1);

    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == NULL)
    {
        ret = -1;
        goto __exit;
    }

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s/activate_agent/", bk_get_bk_server_url(1));
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
    bk_sconf_get_channel_name(tmp_channel);
    data_len = os_snprintf(post_data, POST_DATA_MAX_SIZE, "{\"channel\":\"%s\",", tmp_channel);
    //agent_param reserved for further development
    data_len += os_snprintf(post_data+data_len, POST_DATA_MAX_SIZE, "\"agent_param\": {\"mode\":\"%s\"", update_info);
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

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s/activate_agent/", bk_get_bk_server_url(1));
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

    data_len = os_snprintf(nfc_post_data, POST_DATA_MAX_SIZE, "{\"channel\":\"%s\",\"agent_type_id\":\"%02x:%02x:%02x:%02x:%02x:%02x:%02x\"}", "todo", 
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
    len = os_snprintf(payload, max_len, "{\"channel\":\"%s\"}", uid_str);
    BK_LOGI(TAG, "ori channel name:%s, %d\r\n", uid_str, len);
    return len;
}

void  bk_sconf_prase_agent_info(char *payload, uint8_t reset)
{
    BK_LOGI(TAG, "%s, begin byte_auto_run, %s\n", __func__, payload);
    cJSON *json = NULL;
    char *tmp_channel = NULL;

    json = cJSON_Parse(payload);
    if (!json)
    {
        BK_LOGE(TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
        return;
    }

    cJSON *channel_name = cJSON_GetObjectItem(json, "channel_name");
    if (channel_name && ((channel_name->type & 0xFF) == cJSON_String))
    {
        tmp_channel = channel_name->valuestring;
        BK_LOGI(TAG, "real channel name:%s\r\n", tmp_channel);
        bk_sconf_save_channel_name(tmp_channel);
    }
    else
    {
        BK_LOGE(TAG, "[Error] not find msg\n");
    }
    cJSON_Delete(json);
    if (!bk_sconf_is_net_pan_configured())
    {
        app_event_send_msg(APP_EVT_CLOSE_BLUETOOTH, 0);
    }
    byte_auto_run(reset);
}

//image recognition mode switch
#define CONFIG_IR_MODE_SWITCH_TASK_PRIORITY 4
static beken_thread_t config_ir_mode_switch_thread_handle = NULL;
uint8_t ir_mode_switching = 0;
extern bool byte_runing;
extern bool g_agent_offline;
extern bool video_started;
extern bk_err_t byte_stop(void);
extern void byte_restart(bool enable_video);
#if (CONFIG_DUAL_SCREEN_AVI_PLAY)
extern uint8_t lvgl_app_init_flag;
#endif
void ir_mode_switch_main(void)
{
    __maybe_unused uint8_t ir_timeout = 0;

    if (!byte_runing) {
        BK_LOGW(TAG, "Please Run AgoraRTC First!");
        goto exit;
    }

    ir_mode_switching = 1;

    if (!video_started) {
        byte_stop();
        bk_sconf_upate_agent_info("vision");
        while (g_agent_offline)
        {
            if (!byte_runing)
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
        //video_turn_on();
	byte_restart(true);

#if (CONFIG_DUAL_SCREEN_AVI_PLAY)
        if (lvgl_app_init_flag == 1) {
            media_app_lvgl_switch_ui(LVGL_UI_DISP_IN_TEXT_AND_IMAGE);
        }
#endif
    } else {
        //video_turn_off();
        byte_stop();
        bk_sconf_upate_agent_info("text");
        byte_restart(false);

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

extern bool first_time_for_network_reconnect;
void bk_sconf_trans_start(void)
{
    if (first_time_for_network_reconnect) {
        first_time_for_network_reconnect = false;
        bk_sconf_start_volc_rtc(1);
    } else
        bk_sconf_start_volc_rtc(0);
}
extern bk_err_t byte_stop(void);
void bk_sconf_trans_stop(void)
{
	byte_stop();
}
