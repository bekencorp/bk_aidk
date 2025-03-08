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


#define TAG "bk_sconf"
#define RCV_BUF_SIZE            1024*3
#define SEND_HEADER_SIZE        1024
#define POST_DATA_MAX_SIZE  1024*2
#define MAX_URL_LEN         256
extern char agora_appid[50];
extern char channel_name[50];
extern char *app_id_record;

static int bk_agora_ai_agent_start_rsp_parse(char *text)
{
    cJSON *json = NULL;
    __maybe_unused char *StatusCode = NULL, *state, *detail, *reason;
    __maybe_unused int create_ts;

    json = cJSON_Parse(text);
    if (!json)
    {
        BK_LOGE(TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
        return BK_FAIL;
    }

    cJSON *code = cJSON_GetObjectItem(json, "status");
    if (code && ((code->type & 0xFF) == cJSON_String))
    {
        StatusCode = os_strdup(code->valuestring);
    }
    else
    {
        BK_LOGE(TAG, "[Error] not find statusCode\n");
        goto fail;
    }
    BK_LOGE(TAG, "%s, StatusCode:%s\r\n", __func__, StatusCode);

    cJSON *app_id = cJSON_GetObjectItem(json, "agent_id");
    if (app_id && ((app_id->type & 0xFF) == cJSON_String))
    {
        app_id_record = os_strdup(app_id->valuestring);
        BK_LOGI(TAG, "app_id:%s\r\n", app_id_record);
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

int bk_agora_ai_agent_start(char *channel)
{
    struct webclient_session *session = NULL;
    __maybe_unused char *buffer = NULL, *post_data = NULL;
    char generate_url[256] = {0};
    __maybe_unused int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0;

    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == NULL)
    {
        goto __exit;
    }

    /*Generate https url*/

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "xxx");
    if ((url_len < 0) || (url_len >= MAX_URL_LEN))
    {
        BK_LOGE(TAG, "URL len overflow\r\n");
        return BK_FAIL;
    }

    /*Generate https header*/
    webclient_header_fields_add(session, "Content-Length: %d\r\n", os_strlen(channel));
    webclient_header_fields_add(session, "Content-Type: application/json\r\n");

    buffer = (char *) web_malloc(RCV_BUF_SIZE);
    if (buffer == NULL)
    {
        BK_LOGE(TAG, "no memory for receive response buffer.\n");
        goto __exit;
    }
    os_memset(buffer, 0, RCV_BUF_SIZE);

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, generate_url, channel, os_strlen(channel))) != 200)
    {
        BK_LOGE(TAG, "webclient POST request failed, response(%d) error.\n", resp_status);
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

    resp_status = bk_agora_ai_agent_start_rsp_parse(buffer);
__exit:
    if (session)
    {
        webclient_close(session);
    }

    if (buffer)
    {
        web_free(buffer);
    }
#if 0
    if (post_data)
    {
        os_free(post_data);
    }
#endif
    return BK_OK;
}

extern int demo_save_wifi_auto_restart_info(netif_if_t type, void *val);
static int bk_genie_netif_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    netif_event_got_ip4_t *got_ip;
    __maybe_unused wifi_sta_config_t sta_config = {0};

    switch (event_id)
    {
        case EVENT_NETIF_GOT_IP4:
            got_ip = (netif_event_got_ip4_t *)event_data;
            BK_LOGI(TAG, "%s got ip %s.\n", got_ip->netif_if == NETIF_IF_STA ? "STA" : "unknown netif", got_ip->ip);
            bk_wifi_sta_get_config(&sta_config);
            demo_save_wifi_auto_restart_info(NETIF_IF_STA, &sta_config);
            //extern void agora_auto_run(void);
            //agora_auto_run();
            break;
        default:
            BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}

static int bk_genie_wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    wifi_event_sta_disconnected_t *sta_disconnected;
    wifi_event_sta_connected_t *sta_connected;

    switch (event_id)
    {
        case EVENT_WIFI_STA_CONNECTED:
            sta_connected = (wifi_event_sta_connected_t *)event_data;
            BK_LOGI(TAG, "STA connected to %s\n", sta_connected->ssid);
            break;

        case EVENT_WIFI_STA_DISCONNECTED:
            sta_disconnected = (wifi_event_sta_disconnected_t *)event_data;
            BK_LOGI(TAG, "STA disconnected, reason(%d)\n", sta_disconnected->disconnect_reason);
            break;

        default:
            BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}


static void event_handler_init(void)
{
    BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, bk_genie_wifi_event_cb, NULL));
    BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_NETIF, EVENT_ID_ALL, bk_genie_netif_event_cb, NULL));
}

extern int demo_wifi_auto_restart(void);
int bk_genie_smart_config_init(void)
{
    int flag;

    event_handler_init();
    flag = demo_wifi_auto_restart();
    if (flag != 0x71l && flag != 0x73l)
    {
        wifi_boarding_adv_start();
    }

    return 0;
}

extern void demo_wifi_erase_auto_restart_info(void);
void bk_genie_smart_config_cli(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    demo_wifi_erase_auto_restart_info();
}

