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
#if CONFIG_BK_SMART_CONFIG
#include "bk_smart_config.h"
#endif
// #if CONFIG_AGORA_IOT_SDK
// #include "bk_smart_config_agora_adapter.h"
// #endif
#if CONFIG_NET_PAN
#include "pan_service.h"
#endif
#include "app_event.h"
#include "bk_factory_config.h"

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
bool enable_ble_split_pkt = false;

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
extern uint8_t network_disc_evt_posted;
static int bk_genie_wifi_sta_connect(char *ssid, char *key)
{
    int ssid_len, key_len;

    wifi_sta_config_t sta_config = {0};

    ssid_len = os_strlen(ssid);

    if (32 < ssid_len)
    {
        LOGE("ssid name more than 32 Bytes\r\n");
        return BK_FAIL;
    }

    os_strcpy(sta_config.ssid, ssid);

    key_len = os_strlen(key);

    if (64 < key_len || key_len < 8)
    {
        LOGE("Invalid passphrase, expected: 8..63\r\n");
        return BK_FAIL;
    }

    os_strcpy(sta_config.password, key);
    network_disc_evt_posted = 0;
#if CONFIG_STA_AUTO_RECONNECT
    sta_config.auto_reconnect_count = 5;
    sta_config.disable_auto_reconnect_after_disconnect = true;
#endif
    LOGE("ssid:%s key:%s\r\n", sta_config.ssid, sta_config.password);
    BK_LOG_ON_ERR(bk_wifi_sta_set_config(&sta_config));
    BK_LOG_ON_ERR(bk_wifi_sta_start());

    return BK_OK;
}


#include "components/bk_nfc.h"
typedef struct
{
	uint32_t event;
	uint8_t *param;
} nfc_msg_t;

#define NFC_GOT_ID  1

uint8_t nfc_callback(uint8_t event_param, void*card_id)
{
    nfc_msg_t msg;

    switch(event_param)
    {
        case NFC_GOT_ID:
        {
            msg.event = BOARDING_OP_NFC_GOT_ID;
            msg.param = (uint8_t*)(card_id);
            bk_genie_send_msg((bk_genie_msg_t *)&msg);
        }
        break;

        default :
        break;
    }
    return 0;
}

static int bk_genie_wlan_scan_done_handler(void *arg, event_module_t event_module,
								  int event_id, void *event_data)
{
    wifi_scan_result_t scan_result = {0};
    char payload[200];
    uint16 len = 0;
    int i = 0, j = 0;

    BK_LOG_ON_ERR(bk_wifi_scan_get_result(&scan_result));
    if (scan_result.ap_num == 0)
        goto exit;

again:
    os_memset(payload, 0, 200);
    len = os_snprintf(payload, 200, "[");
    for (i = j; i < scan_result.ap_num; i++) {
        if (!os_strlen(scan_result.aps[i].ssid))
            continue;
        if ((len + 5 + os_strlen(scan_result.aps[i].ssid)) > 200) {
            j = i;
            break;
        }
        if ((i != 0) && (len != 1))
            len += os_snprintf(payload+len, 200, ",");
        len += os_snprintf(payload+len, 200, "\"%s\"", scan_result.aps[i].ssid);
        j = i + 1;
    }
    len += os_snprintf(payload+len, 200, "]");
    LOGI("upload scan_rst %s, sended:%d, total:%d\r\n", payload, j, scan_result.ap_num);
    if ((j >= scan_result.ap_num) || (enable_ble_split_pkt == false))
        bk_genie_boarding_event_notify_with_data(BOARDING_OP_START_WIFI_SCAN, 0, payload, len);
    else {
        bk_genie_boarding_event_notify_with_data(BOARDING_OP_START_WIFI_SCAN, 1, payload, len);
        goto again;
    }      

exit:
    bk_wifi_scan_free_result(&scan_result);

    return BK_OK;
}

static void bk_genie_message_handle(void)
{
    bk_err_t ret = BK_OK;
    bk_genie_msg_t msg;
#if (CONFIG_NFC_ENABLE)
    nfc_event_callback_register(nfc_callback);
#endif
    while (1)
    {

        ret = rtos_pop_from_queue(&db_info->queue, &msg, BEKEN_WAIT_FOREVER);

        if (kNoErr == ret)
        {
            switch (msg.event)
            {
                case BOARDING_OP_STATION_START:
                {
                    //for BOARDING_OP_STATION_START,msg.param:
                    //0 means start sta connect
                    //low 16bit non-zero means got ip and need to notify apk, high 16bit store netif_index
                    LOGI("BOARDING_OP_STATION_START, param:%d\n", msg.param);
			if (!msg.param) {
                        bk_genie_boarding_info_t *bk_genie_boarding_info = bk_genie_get_boarding_info();
                        bk_genie_wifi_sta_connect(bk_genie_boarding_info->boarding_info.ssid_value,
                                                  bk_genie_boarding_info->boarding_info.password_value);
                        bk_event_unregister_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE,
                                                                   bk_genie_wlan_scan_done_handler);
			} else {
                        netif_ip4_config_t ip4_config;
                        netif_if_t netif_idx;

                        netif_idx = (msg.param >> 16) & 0xffff;
                        os_memset(&ip4_config, 0x0, sizeof(netif_ip4_config_t));
                        bk_netif_get_ip4_config(netif_idx, &ip4_config);
                        LOGI("ip: %s\n", ip4_config.ip);
                        bk_genie_boarding_event_notify_with_data(BOARDING_OP_STATION_START, BK_OK, ip4_config.ip, strlen(ip4_config.ip));
                    }
                }
                break;

                case BOARDING_OP_START_WIFI_SCAN:
                {
                    LOGI("BOARDING_OP_START_WIFI_SCAN\n");
			if (msg.param) {
                        if (*(uint8_t *)msg.param == 1)
                            enable_ble_split_pkt = true;
                        os_free((void *)(msg.param));
			}
                    bk_event_register_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE,
                    						   bk_genie_wlan_scan_done_handler, NULL);
                    BK_LOG_ON_ERR(bk_wifi_scan_start(NULL));
                }
                break;

                case BOARDING_OP_SET_AGENT_INFO:
                {
                    LOGI("BOARDING_OP_SET_AGENT_INFO\n");
                    char payload[256] = {0};
                    __maybe_unused uint16_t len = 0;
                    len = bk_sconf_send_agent_info(payload, 256);
                    bk_genie_boarding_event_notify_with_data(BOARDING_OP_SET_AGENT_INFO, 0, payload, len);
                }
                break;

                case BOARDING_OP_AGENT_RSP:
                {
                    LOGI("BOARDING_OP_AGENT_RSP\n");
                    bk_sconf_prase_agent_info((char *)msg.param, 1);
                    if (msg.param)
                        os_free((void *)(msg.param));
                }
                break;

                case BOARDING_OP_BLE_DISABLE:
                {
                    LOGI("close bluetooth ing\n");
#if CONFIG_BLUETOOTH
                    bk_genie_boarding_deinit();
                    bk_bluetooth_deinit();
                    LOGI("close bluetooth finish!\r\n");
#endif
                }
                break;

                case BOARDING_OP_NET_PAN_START:
                {
                    LOGI("DBEVT_NET_PAN_REQUEST\n");
                    int status = 1;
#if CONFIG_NET_PAN
                    bk_bt_enter_pairing_mode(1);
                    status = 0;
#endif
                    uint8_t bt_mac[6];
                    bk_get_mac(bt_mac, MAC_TYPE_BLUETOOTH);
                    bk_genie_boarding_event_notify_with_data(BOARDING_OP_NET_PAN_START, status, (char *)bt_mac, 6);
                }
                break;

#if CONFIG_STA_AUTO_RECONNECT
                case BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME:
                {
                extern uint8_t first_time_for_network_provisioning;
                    LOGI("BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME, %d\r\n", *(uint8_t *)(msg.param));
                    if (*(uint8_t *)(msg.param))
                        first_time_for_network_provisioning = false;
                    if (msg.param)
                        os_free((void *)(msg.param));
                }
                break;
#endif

                case BOARDING_OP_NFC_GOT_ID:
                {
                    uint8_t nfc_id[7];
                    nfc_msg_t nfc_info;
                    nfc_info.param = (uint8_t *)(msg.param);
                    os_memcpy(nfc_id, nfc_info.param, 7);
                    LOGI("DBEVT_NFC_GOT_ID: [%02x:%02x:%02x:%02x:%02x:%02x:%02x]\r\n", nfc_id[0], nfc_id[1], nfc_id[2],\
                    nfc_id[3], nfc_id[4], nfc_id[5], nfc_id[6]);

                    bk_sconf_post_nfc_id(nfc_id);
                }
                break;

#if CONFIG_BK_MODEM
                case BOARDING_OP_START_BK_MODEM:
                {
extern bk_err_t bk_modem_init(void);
                    ret = bk_modem_init();
			if (ret) {
                        bk_modem_init();
			}
                }
                    break;
#endif
                case BOARDING_OP_SYNC_SUPPORTED_ENGINE:
                {
                    uint8_t val = bk_sconf_get_supported_engine();
                    bk_genie_boarding_event_notify_with_data(BOARDING_OP_SYNC_SUPPORTED_ENGINE, 0, (char *)&val, 1);
                }
                break;

                case BOARDING_OP_SYNC_SUPPORTED_NETWORK:
                {
			uint8_t *val = NULL, len = 0;
                    val = bk_sconf_get_supported_network(&len);
                    bk_genie_boarding_event_notify_with_data(BOARDING_OP_SYNC_SUPPORTED_NETWORK, 0, (char *)val, len);
                    if (val)
                        os_free(val);
                }
                break;

                default:
                {
                    LOGI("%s %d, do nothing\r\n", __func__, msg.event);
                }
                break;
            }
        }
    }

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
                             4096,
                             NULL);

    if (ret != BK_OK)
    {
        LOGE("create media major thread fail\n");
        goto error;
    }

    db_info->enabled = BK_TRUE;

    LOGE("%s success\n", __func__);

    return;

error:

    LOGE("%s fail\n", __func__);
}

