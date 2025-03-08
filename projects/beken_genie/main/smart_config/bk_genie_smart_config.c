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
#if (CONFIG_EASY_FLASH && CONFIG_EASY_FLASH_V4)
#include "bk_ef.h"
#endif
#include "pan_service.h"

#define TAG "bk_sconf"
#define RCV_BUF_SIZE            256
#define SEND_HEADER_SIZE           1024
#define POST_DATA_MAX_SIZE  1024
#define MAX_URL_LEN         256
extern char *app_id_record;
static bool smart_config_running = false;
char *app_id_record = NULL;
char *channel_name_record = NULL;

extern int demo_save_wifi_auto_restart_info(netif_if_t type, void *val);
extern void agora_auto_run(void);
static int bk_genie_sconf_netif_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    netif_event_got_ip4_t *got_ip;
    bk_genie_msg_t msg;
    __maybe_unused wifi_sta_config_t sta_config = {0};

    switch (event_id)
    {
        case EVENT_NETIF_GOT_IP4:
            got_ip = (netif_event_got_ip4_t *)event_data;
            BK_LOGI(TAG, "%s got ip %s.\n", got_ip->netif_if == NETIF_IF_STA ? "STA" : "unknown netif", got_ip->ip);
            if (smart_config_running)
            {
                bk_wifi_sta_get_config(&sta_config);
                demo_save_wifi_auto_restart_info(NETIF_IF_STA, &sta_config);
                msg.event = DBEVT_WIFI_STATION_CONNECTED;
                bk_genie_send_msg(&msg);

                msg.event = DBEVT_START_AGORA_AGENT_START;
                bk_genie_send_msg(&msg);
            }
            else
            {
                bk_genie_agent_info_t info = {0};
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
            msg.event = DBEVT_WIFI_STATION_DISCONNECTED;
            bk_genie_send_msg(&msg);
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

extern void demo_wifi_erase_auto_restart_info(void);
extern int demo_wifi_auto_restart(void);
extern bk_err_t agora_stop(void);
void bk_genie_prepare_for_smart_config(void)
{
    smart_config_running = true;
    agora_stop();
    demo_wifi_erase_auto_restart_info();
    bk_genie_erase_agent_info();
    wifi_boarding_adv_start();
#if CONFIG_NET_PAN
    bk_bt_enter_pairing_mode();
#endif
}

int bk_genie_smart_config_init(void)
{
    int flag;

    event_handler_init();
    flag = demo_wifi_auto_restart();

#if CONFIG_NET_PAN
    if (flag == 0x74l)
    {
        bt_start_pan_reconnect();
        return 0;
    }
#endif

    if (flag != 0x71l && flag != 0x73l)
    {
        bk_genie_prepare_for_smart_config();
    }

    return 0;
}

extern void demo_wifi_erase_auto_restart_info(void);
void bk_genie_smart_config_cli(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    demo_wifi_erase_auto_restart_info();
}

void bk_genie_erase_agent_info(void)
{
    bk_genie_agent_info_t info_tmp = {0};

#if (CONFIG_EASY_FLASH && CONFIG_EASY_FLASH_V4)
    bk_set_env_enhance("d_agent_info", (const void *)&info_tmp, sizeof(bk_genie_agent_info_t));
#endif
}

int bk_genie_save_agent_info(char *appid, char *channel_name)
{
    bk_genie_agent_info_t info_tmp = {0};

    info_tmp.valid = 1;
    os_strcpy(info_tmp.appid, appid);
    os_strcpy(info_tmp.channel_name, channel_name);
#if (CONFIG_EASY_FLASH && CONFIG_EASY_FLASH_V4)
    bk_set_env_enhance("d_agent_info", (const void *)&info_tmp, sizeof(bk_genie_agent_info_t));
#endif
    return 0;
}

int bk_genie_get_agent_info(bk_genie_agent_info_t *info)
{
    bk_genie_agent_info_t info_tmp = {0};

#if (CONFIG_EASY_FLASH && CONFIG_EASY_FLASH_V4)
    if (bk_get_env_enhance("d_agent_info", (void *)&info_tmp, sizeof(bk_genie_agent_info_t)) <= 0)
    {
        return -1;
    }
#endif
    os_memcpy(info, &info_tmp, sizeof(bk_genie_agent_info_t));
    return 0;
}

#define BEKEN_SERVER_URL "http://47.102.43.223:8990/api/agora_service/activate_agent/"
int bk_genie_wakeup_agent(void)
{
    struct webclient_session *session = NULL;
    char *buffer = NULL, *post_data = NULL;
    char generate_url[256] = {0};
    int url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0;

    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == NULL)
    {
        goto __exit;
    }

    url_len = os_snprintf(generate_url, MAX_URL_LEN, "%s", BEKEN_SERVER_URL);
    if ((url_len < 0) || (url_len >= MAX_URL_LEN))
    {
        BK_LOGE(TAG, "URL len overflow\r\n");
        return BK_FAIL;
    }

    /*Generate data*/
    post_data = os_malloc(POST_DATA_MAX_SIZE);
    if (post_data == NULL)
    {
        BK_LOGE(TAG, "no memory for post_data buffer\n");
        goto __exit;
    }
    os_memset(post_data, 0, POST_DATA_MAX_SIZE);

    data_len = os_snprintf(post_data, POST_DATA_MAX_SIZE, "{\"channel\":\"%s\"}", channel_name_record);
    BK_LOGI(TAG, "%s, %s\r\n", __func__, post_data);

    webclient_header_fields_add(session, "Content-Length: %d\r\n", os_strlen(post_data));
    webclient_header_fields_add(session, "Content-Type: application/json\r\n");

    buffer = (char *) web_malloc(RCV_BUF_SIZE);
    if (buffer == NULL)
    {
        BK_LOGE(TAG, "no memory for receive response buffer.\n");
        goto __exit;
    }
    os_memset(buffer, 0, RCV_BUF_SIZE);

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, generate_url, post_data, data_len)) != 200)
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

    return BK_OK;
}

