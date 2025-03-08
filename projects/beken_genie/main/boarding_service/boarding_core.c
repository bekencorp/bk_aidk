#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>

#include <modules/wifi.h>
#include <components/event.h>
#include <components/netif.h>
#include "cJSON.h"
#include "components/bk_uid.h"

#include "wifi_boarding_utils.h"
#include "bk_genie_comm.h"
#include "boarding_service.h"
#include "components/bluetooth/bk_dm_bluetooth.h"
#include "cli.h"
#include "bk_genie_smart_config.h"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define TAG "db-core"

typedef struct
{
    uint32_t enabled : 1;
    uint32_t service : 6;

    char *id;
    beken_thread_t thd;
    beken_queue_t queue;
} bk_genie_info_t;

bk_genie_info_t *db_info = NULL;

const bk_genie_service_interface_t *bk_genie_current_service = NULL;


bk_err_t bk_genie_send_msg(bk_genie_msg_t *msg)
{
    bk_err_t ret = BK_OK;

    if (db_info->queue)
    {
        ret = rtos_push_to_queue(&db_info->queue, msg, BEKEN_NO_WAIT);

        if (BK_OK != ret)
        {
            LOGE("%s failed\n", __func__);
            return BK_FAIL;
        }

        return ret;
    }

    return ret;
}

extern char *app_id_record;
extern char *channel_name_record;

static int bk_genie_wifi_sta_connect(char *ssid, char *key)
{
    int len;

    wifi_sta_config_t sta_config = {0};

    len = os_strlen(key);

    if (32 < len)
    {
        LOGE("ssid name more than 32 Bytes\r\n");
        return BK_FAIL;
    }

    os_strcpy(sta_config.ssid, ssid);

    len = os_strlen(key);

    if (64 < len)
    {
        LOGE("key more than 64 Bytes\r\n");
        return BK_FAIL;
    }

    os_strcpy(sta_config.password, key);

    LOGE("ssid:%s key:%s\r\n", sta_config.ssid, sta_config.password);
    BK_LOG_ON_ERR(bk_wifi_sta_set_config(&sta_config));
    BK_LOG_ON_ERR(bk_wifi_sta_start());

    return BK_OK;
}

extern void agora_auto_run(void);
static void bk_genie_message_handle(void)
{
    bk_err_t ret = BK_OK;
    bk_genie_msg_t msg;

    while (1)
    {

        ret = rtos_pop_from_queue(&db_info->queue, &msg, BEKEN_WAIT_FOREVER);

        if (kNoErr == ret)
        {
            switch (msg.event)
            {
                case DBEVT_WIFI_STATION_CONNECT:
                {
                    LOGI("DBEVT_WIFI_STATION_CONNECT\n");
                    bk_genie_boarding_info_t *bk_genie_boarding_info = (bk_genie_boarding_info_t *) msg.param;
                    bk_genie_wifi_sta_connect(bk_genie_boarding_info->boarding_info.ssid_value,
                                              bk_genie_boarding_info->boarding_info.password_value);
                }
                break;

                case DBEVT_WIFI_STATION_CONNECTED:
                {
                    LOGI("DBEVT_WIFI_STATION_CONNECTED\n");
                    netif_ip4_config_t ip4_config;
                    extern uint32_t uap_ip_is_start(void);

                    os_memset(&ip4_config, 0x0, sizeof(netif_ip4_config_t));
                    bk_netif_get_ip4_config(NETIF_IF_AP, &ip4_config);

                    if (uap_ip_is_start())
                    {
                        bk_netif_get_ip4_config(NETIF_IF_AP, &ip4_config);
                    }
                    else
                    {
                        bk_netif_get_ip4_config(NETIF_IF_STA, &ip4_config);
                    }

                    LOGI("ip: %s\n", ip4_config.ip);

                    bk_genie_boarding_event_notify_with_data(BOARDING_OP_STATION_START, BK_OK, ip4_config.ip, strlen(ip4_config.ip));
                }
                break;

                case DBEVT_START_AGORA_AGENT_START:
                {
                    LOGI("DBEVT_START_AGORA_AGENT_START\n");
                    unsigned char uid[32] = {0};
                    char uid_str[65] = {0};
                    char payload[128] = {0};
                    uint16 len = 0;

                    bk_uid_get_data(uid);
                    for (int i = 0; i < 24; i++)
                    {
                        sprintf(uid_str + i * 2, "%02x", uid[i]);
                    }
                    len = os_snprintf(payload, 128, "{\"channel\":\"%s\"}", uid_str);
                    LOGI("ori channel name:%s, %s, %d\r\n", uid_str, payload, len);
                    bk_genie_boarding_event_notify_with_data(BOARDING_OP_SET_AGORA_AGENT_INFO, 0, payload, len);
                }
                break;

                case DBEVT_START_AGORA_AGENT_RSP:
                {
                    LOGI("DBEVT_START_AGORA_AGENT_RSP\n");
                    cJSON *json = NULL;

                    json = cJSON_Parse((char *)(msg.param));
                    if (!json)
                    {
                        LOGE("Error before: [%s]\n", cJSON_GetErrorPtr());
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
                    cJSON *app_id = cJSON_GetObjectItem(json, "app_id");
                    if (app_id && ((app_id->type & 0xFF) == cJSON_String))
                    {
                        app_id_record = os_strdup(app_id->valuestring);
                    }
                    else
                    {
                        LOGE("[Error] not find msg\n");
                    }

                    cJSON *channel_name = cJSON_GetObjectItem(json, "channel_name");
                    if (channel_name && ((channel_name->type & 0xFF) == cJSON_String))
                    {
                        channel_name_record = os_strdup(channel_name->valuestring);
                        LOGI("real channel name:%s\r\n", channel_name_record);
                    }
                    else
                    {
                        LOGE("[Error] not find msg\n");
                    }
                    cJSON_Delete(json);
                    if (app_id_record && channel_name_record)
                    {
                        bk_genie_save_agent_info(app_id_record, channel_name_record);
                        LOGI("begin agora_auto_run\n");
                        agora_auto_run();
                    }
                    break;
                }
                break;

                case DBEVT_WIFI_STATION_DISCONNECTED:
                {
                    LOGI("DBEVT_WIFI_STATION_DISCONNECTED\n");
                }
                break;

                case DBEVT_WIFI_SOFT_AP_TURNING_ON:
                {
                    LOGI("DBEVT_WIFI_SOFT_AP_TURNING_ON\n");
#if 0
                    bk_genie_boarding_info_t *bk_genie_boarding_info = (bk_genie_boarding_info_t *) msg.param;
                    int ret = bk_genie_wifi_soft_ap_start(bk_genie_boarding_info->boarding_info.ssid_value,
                                                          bk_genie_boarding_info->boarding_info.password_value,
                                                          bk_genie_boarding_info->channel);

                    if (ret == BK_OK)
                    {
                        bk_genie_boarding_event_notify(BOARDING_OP_SOFT_AP_START, EVT_STATUS_OK);
                    }
                    else
                    {
                        bk_genie_boarding_event_notify(BOARDING_OP_SOFT_AP_START, EVT_STATUS_ERROR);
                    }

#endif
                }
                break;

#if 0

                case DBEVT_LAN_UDP_SERVICE_START_REQUEST:
                {
                    LOGI("DBEVT_LAN_UDP_SERVICE_START_REQUEST\n");

                    if (db_info->service != bk_genie_SERVICE_NONE)
                    {
                        LOGW("DBEVT_LAN_UDP_SERVICE_START_REQUEST service: %d already start up\n", db_info->service);
                        break;
                    }

                    db_info->service = bk_genie_SERVICE_LAN_UDP;

                    bk_genie_cmd_server_init();
                    bk_genie_udp_service_init();

                }
                break;

                case DBEVT_LAN_UDP_SERVICE_START_RESPONSE:
                {
                    LOGI("DBEVT_LAN_UDP_SERVICE_START_RESPONSE\n");

                    bk_genie_sdp_start("doorbell-udp", bk_genie_CMD_PORT, bk_genie_UDP_IMG_PORT, bk_genie_UDP_AUD_PORT);

                    bk_genie_boarding_event_notify(BOARDING_OP_SERVICE_UDP_START, BK_OK);
                }
                break;

                case DBEVT_LAN_TCP_SERVICE_START_REQUEST:
                {
                    LOGI("DBEVT_LAN_TCP_SERVICE_START_REQUEST\n");

                    if (db_info->service != bk_genie_SERVICE_NONE)
                    {
                        LOGW("DBEVT_LAN_TCP_SERVICE_START_REQUEST service: %d already start up\n", db_info->service);
                        break;
                    }

                    db_info->service = bk_genie_SERVICE_LAN_TCP;

                    bk_genie_cmd_server_init();
                    bk_genie_tcp_service_init();

                }
                break;

                case DBEVT_LAN_TCP_SERVICE_START_RESPONSE:
                {
                    LOGI("DBEVT_LAN_TCP_SERVICE_START_RESPONSE\n");

                    bk_genie_sdp_start("doorbell-tcp", bk_genie_CMD_PORT, bk_genie_TCP_IMG_PORT, bk_genie_TCP_AUD_PORT);

                    bk_genie_boarding_event_notify(BOARDING_OP_SERVICE_TCP_START, BK_OK);
                }
                break;

                case DBEVT_P2P_CS2_SERVICE_START_REQUEST:
                {
#ifdef CONFIG_INTEGRATION_bk_genie_CS2
                    LOGI("DBEVT_P2P_CS2_SERVICE_START_REQUEST\n");

                    if (db_info->service != bk_genie_SERVICE_NONE)
                    {
                        LOGW("DBEVT_P2P_CS2_SERVICE_START_REQUEST service: %d already start up\n", db_info->service);
                        break;
                    }

                    db_info->service = bk_genie_SERVICE_P2P_CS2;

                    p2p_cs2_key_t *key = (p2p_cs2_key_t *)msg.param;

                    bk_genie_current_service = get_bk_genie_cs2_service_interface();
                    bk_genie_current_service->init(key);
#endif
                }
                break;

                case DBEVT_P2P_CS2_SERVICE_START_RESPONSE:
                {
                    bk_genie_boarding_event_notify(BOARDING_OP_SRRVICE_CS2_START, BK_OK);
                }
                break;

                case DBEVT_START_BOARDING_EVENT:
                {
                    uint16_t opcode = msg.param & 0xFFFF;
                    int status = msg.param >> 16;
                    bk_genie_boarding_event_notify(opcode, status);
                }
                break;
#endif

                case DBEVT_BLE_DISABLE:
                {
                    LOGI("close bluetooth ing\n");
#if CONFIG_BLUETOOTH
                    bk_bluetooth_deinit();
                    LOGI("close bluetooth finish!\r\n");
#endif
                }
                break;
#if 0

                case DBEVT_REMOTE_DEVICE_CONNECTED:
                {
                    if (db_info->service == bk_genie_SERVICE_LAN_UDP)
                    {
                        bk_genie_udp_update_remote_address((in_addr_t)msg.param);
                        bk_genie_sdp_reload();
                    }
                    else if (db_info->service == bk_genie_SERVICE_LAN_TCP)
                    {
                        bk_genie_sdp_reload();
                    }
                }
                break;

                case DBEVT_REMOTE_DEVICE_DISCONNECTED:
                {
                    bk_genie_video_transfer_turn_off();
                    bk_genie_audio_turn_off();

                    if (db_info->service == bk_genie_SERVICE_LAN_UDP)
                    {
                        bk_genie_sdp_start("doorbell-udp", bk_genie_CMD_PORT, bk_genie_UDP_IMG_PORT, bk_genie_UDP_AUD_PORT);
                    }
                    else if (db_info->service == bk_genie_SERVICE_LAN_TCP)
                    {
                        bk_genie_sdp_start("doorbell-tcp", bk_genie_CMD_PORT, bk_genie_TCP_IMG_PORT, bk_genie_TCP_AUD_PORT);
                    }
                }
                break;

                case DBEVT_IMAGE_TCP_SERVICE_DISCONNECTED:
                {
                    bk_genie_video_transfer_turn_off();

                    if (db_info->service == bk_genie_SERVICE_LAN_UDP)
                    {
                        bk_genie_sdp_start("doorbell-udp", bk_genie_CMD_PORT, bk_genie_UDP_IMG_PORT, bk_genie_UDP_AUD_PORT);
                    }
                    else if (db_info->service == bk_genie_SERVICE_LAN_TCP)
                    {
                        bk_genie_sdp_start("doorbell-tcp", bk_genie_CMD_PORT, bk_genie_TCP_IMG_PORT, bk_genie_TCP_AUD_PORT);
                    }
                }
                break;
#endif

                case DBEVT_EXIT:
                    goto exit;
                    break;

                default:
                    break;
            }
        }
    }

exit:

#if 0
    bk_genie_sdp_stop();
#endif
    /* delate msg queue */
    ret = rtos_deinit_queue(&db_info->queue);

    if (ret != kNoErr)
    {
        LOGE("delate message queue fail\n");
    }

    db_info->queue = NULL;

    LOGE("delate message queue complete\n");

    /* delate task */
    rtos_delete_thread(NULL);

    db_info->thd = NULL;

    LOGE("delate task complete\n");
}


void bk_genie_core_init(void)
{
    bk_err_t ret = BK_OK;

    if (db_info == NULL)
    {
        db_info = os_malloc(sizeof(bk_genie_info_t));

        if (db_info == NULL)
        {
            LOGE("%s, malloc db_info failed\n", __func__);
            goto error;
        }

        os_memset(db_info, 0, sizeof(bk_genie_info_t));
    }


    if (db_info->queue != NULL)
    {
        ret = BK_FAIL;
        LOGE("%s, db_info->queue allready init, exit!\n", __func__);
        goto error;
    }

    if (db_info->thd != NULL)
    {
        ret = BK_FAIL;
        LOGE("%s, db_info->thd allready init, exit!\n", __func__);
        goto error;
    }

    ret = rtos_init_queue(&db_info->queue,
                          "db_info->queue",
                          sizeof(bk_genie_msg_t),
                          10);

    if (ret != BK_OK)
    {
        LOGE("%s, ceate doorbell message queue failed\n");
        goto error;
    }

    ret = rtos_create_thread(&db_info->thd,
                             BEKEN_DEFAULT_WORKER_PRIORITY,
                             "db_info->thd",
                             (beken_thread_function_t)bk_genie_message_handle,
                             2560,
                             NULL);

    if (ret != BK_OK)
    {
        LOGE("create media major thread fail\n");
        goto error;
    }

    extern bool ate_is_enabled(void);

    if (!ate_is_enabled())
    {
        bk_genie_boarding_init();
    }
    else
    {
        LOGI("ATE is enable, ble adv disable!!!!!! \r\n");
    }


    db_info->enabled = BK_TRUE;

    LOGE("%s success\n", __func__);

    return;

error:

    LOGE("%s fail\n", __func__);
}

