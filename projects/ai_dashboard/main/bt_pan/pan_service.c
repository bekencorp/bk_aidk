#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "pan_service.h"
#include "bt_comm_list.h"
#include "components/bluetooth/bk_dm_bluetooth_types.h"
#include "components/bluetooth/bk_dm_bt_types.h"
#include "components/bluetooth/bk_dm_bt.h"
#include "components/bluetooth/bk_dm_pan.h"
#include "storage/bluetooth_storage.h"
#include "pan_user_config.h"
#include "bt_manager.h"
#include "net.h"
#include "panif.h"
#include "netif/etharp.h"
#include "app_event.h"
#include "hidd_service.h"

#define TAG "pan"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


//#define PAN_GPIO_DEBUG   //GPIO debug

#ifdef PAN_GPIO_DEBUG
#define PAN_NET_OUTPUT()           do { GPIO_UP(32); GPIO_DOWN(32);} while (0)
#define PAN_TX()                   do { GPIO_UP(33); GPIO_DOWN(33);} while (0)
#define PAN_NET_INPUT_START()      do { GPIO_DOWN(34); GPIO_UP(34);} while (0)
#define PAN_NET_INPUT_END()        do { GPIO_DOWN(34); } while (0)
#define PAN_TX_DONE()              do { GPIO_UP(35); GPIO_DOWN(35);} while (0)
#else
#define PAN_NET_OUTPUT()
#define PAN_TX()
#define PAN_NET_INPUT_START()
#define PAN_NET_INPUT_END()
#define PAN_TX_DONE()
#endif


enum
{
    BT_PAN_MSG_NULL = 0,
    BT_PAN_DATA_IND_MSG = 1,
    BT_PAN_TX_DONE_IND_MSG = 2,
    BT_PAN_TX_MSG = 3,
    BT_PAN_EXIT_MSG = 4,
};

typedef struct
{
    uint8_t type;
    uint16_t len;
    char *data;
} bt_pan_service_msg_t;

static beken_queue_t bt_pan_service_msg_que = NULL;
static beken_thread_t bt_pan_service_thread_handle = NULL;
uint8_t paired_bt_mac[6] = { 0 };
uint8_t s_pan_state = BK_BTPAN_STATE_DISCONNECTED;

uint8_t s_acl_buf_count = CONFIG_NB_ACL_BUFF;

static bt_comm_list_t *s_pan_tx_list = NULL;
static void *s_pan_tx_lock = NULL;

static uint8_t s_pan_service_already_init = 0;

void pan_show_tx_data_cache_count(void)
{
    LOGI("%s, %d\r\n",__func__, s_pan_tx_list->length);
}

static int pan_push_tx_data_to_list(void *data, uint16_t len)
{
    int ret = 0;

    if (s_pan_tx_list->length > CONFIG_MAX_TX_CACHE_COUNT)
    {
        LOGE("the tx cache count exceeds the limit(%d), discard the tx data!!!\r\n",CONFIG_MAX_TX_CACHE_COUNT);
        return -1;
    }

    uint8_t *p_data = (uint8_t *)psram_malloc(len);
    if (p_data)
    {
        os_memcpy(p_data, data, len);
        bt_comm_list_append(s_pan_tx_list, p_data);
    }
    else
    {
        LOGE("%s, psram_malloc failed!!!\r\n",__func__);
        ret = -1;
    }
    return ret;
}

void bt_pan_reconnect_failure_handler(void)
{
    bt_clear_reconnect_info();
    bt_manager_set_mode(BT_MNG_MODE_IDLE);

extern uint8_t network_disc_evt_posted;
    if (network_disc_evt_posted == 0) {
        app_event_send_msg(APP_EVT_RECONNECT_NETWORK_FAIL, 0);
        network_disc_evt_posted = 1;
    }

#if CONFIG_STA_AUTO_RECONNECT
extern int demo_network_auto_reconnect(bool val);
extern bool smart_config_running;
    if (!smart_config_running) {
        demo_network_auto_reconnect(true);
    }
#endif
}

void bt_start_pan_reconnect(void)
{
    uint8_t recon_addr[6] = {0};

    LOGI("%s\n", __func__);

    if ((bluetooth_storage_get_newest_linkkey_info(recon_addr, NULL)) < 0)
    {
        LOGI("%s can't find linkkey info\n", __func__);
        //bt_manager_set_mode(BT_MNG_MODE_PAIRING);
        bt_pan_reconnect_failure_handler();
    }
    else
    {
        LOGI("%s find addr\n", __func__);
        bt_manager_start_reconnect(recon_addr, 1);
    }
}

void bt_pan_service_main(void *arg)
{
#if 0
    uint8_t recon_addr[6] = {0};
    if ((bluetooth_storage_get_newest_linkkey_info(recon_addr, NULL)) < 0)
    {
        LOGI("%s can't find linkkey info\n", __func__);
        bt_manager_set_mode(BT_MNG_MODE_PAIRING);
    }
    else
    {
        LOGI("%s find addr\n", __func__);
        //bt_manager_start_reconnect(recon_addr, 1);
    }
#endif
    while (1)
    {
        bk_err_t err;
        bt_pan_service_msg_t msg;

        err = rtos_pop_from_queue(&bt_pan_service_msg_que, &msg, BEKEN_WAIT_FOREVER);

        if (kNoErr == err)
        {
            switch (msg.type)
            {
                case BT_PAN_DATA_IND_MSG:
                {
                    eth_data_t *p_data = (eth_data_t *)msg.data;
#if CONFIG_NET_PAN
                    struct netif *netif;
                    struct eth_hdr *ethhdr;
#endif

                    uint8_t *dest = p_data->dest;
                    uint8_t *src = p_data->src;
                    LOGD("PAN_DATA_IND, pro:0x%04x, dest[%02x:%02x:%02x:%02x:%02x:%02x],src[%02x:%02x:%02x:%02x:%02x:%02x], len : %d\r\n",
                         p_data->protocol, dest[0], dest[1], dest[2], dest[3], dest[4], dest[5],
                         src[0], src[1], src[2], src[3], src[4], src[5], p_data->payload_len);
                    LOGD(" data : %x %x- %x %x\r\n", p_data->payload[0], p_data->payload[1], p_data->payload[p_data->payload_len - 2], p_data->payload[p_data->payload_len - 1]);
#if CONFIG_NET_PAN
                    /* PAN Wi-Fi Part */
                    struct pbuf *p = pbuf_alloc(PBUF_RAW, p_data->payload_len + sizeof(struct eth_hdr), PBUF_POOL);

                    if (p == NULL)
                    {
                        LOGI("Failed to allocate pbuf\r\n");
                        os_free(msg.data);
                        break;
                    }

                    if (p_data->payload_len < 0 || p_data->payload_len > p->len - sizeof(struct eth_hdr))
                    {
                        LOGI("Payload too large or invalid for pbuf\r\n");
                        pbuf_free(p);
                        os_free(msg.data);
                        break;
                    }

                    ethhdr = (struct eth_hdr *)p->payload;
                    os_memcpy(ethhdr->dest.addr, p_data->dest, PAN_HWADDR_LEN);
                    os_memcpy(ethhdr->src.addr, p_data->src, PAN_HWADDR_LEN);
                    ethhdr->type = htons(p_data->protocol);
                    netif = net_get_pan_handle();

                    if (netif == NULL || !netif_is_up(netif))
                    {
                        LOGI("Network interface is not ready\r\n");
                        pbuf_free(p);
                        os_free(msg.data);
                        break;
                    }
                    os_memcpy(p->payload + sizeof(struct eth_hdr), p_data->payload, p_data->payload_len);

                    PAN_NET_INPUT_START();
                    panif_input(netif, p);
                    PAN_NET_INPUT_END();
#endif

                    os_free(msg.data);
                }
                break;

                case BT_PAN_TX_DONE_IND_MSG:
                {
                    s_acl_buf_count++;
                }
                case BT_PAN_TX_MSG:
                {
                    if (s_acl_buf_count && (!bt_comm_list_is_empty(s_pan_tx_list)))
                    {
                        PAN_TX();
                        rtos_lock_mutex(&s_pan_tx_lock);
                        eth_data_t *eth_data = bt_comm_list_front(s_pan_tx_list);
                        s_acl_buf_count--;
                        bk_bt_pan_write(paired_bt_mac, eth_data);
                        bt_comm_list_remove(s_pan_tx_list, eth_data);
                        rtos_unlock_mutex(&s_pan_tx_lock);
                    }
                }
                break;

                case BT_PAN_EXIT_MSG:
                    goto exit;
                    break;

                default:
                    LOGD("Unknown message type: %d\r\n", msg.type);
                    break;
            }
        }
    }

exit:

    rtos_deinit_queue(&bt_pan_service_msg_que);
    bt_pan_service_msg_que = NULL;
    bt_pan_service_thread_handle = NULL;
    rtos_delete_thread(NULL);
}

int bt_pan_service_task_init(void)
{
    bk_err_t ret = BK_OK;

    if ((!bt_pan_service_thread_handle) && (!bt_pan_service_msg_que))
    {
        ret = rtos_init_queue(&bt_pan_service_msg_que,
                              "bt_pan_service_msg_que",
                              sizeof(bt_pan_service_msg_t),
                              BT_PAN_SERVICE_MSG_COUNT);

        if (ret != kNoErr)
        {
            LOGE("bt pan service msg queue failed \r\n");
            return BK_FAIL;
        }

        ret = rtos_create_thread(&bt_pan_service_thread_handle,
                                 BT_PAN_SERVICE_TASK_PRIORITY,
                                 "bt_pan_service",
                                 (beken_thread_function_t)bt_pan_service_main,
                                 4096,
                                 (beken_thread_arg_t)0);

        if (ret != kNoErr)
        {
            LOGE("bt pan service task fail \r\n");
            rtos_deinit_queue(&bt_pan_service_msg_que);
            bt_pan_service_msg_que = NULL;
            bt_pan_service_thread_handle = NULL;
        }

        return kNoErr;
    }
    else
    {
        return kInProgressErr;
    }
}

void bt_pan_media_data_ind(eth_data_t *data)
{
    bt_pan_service_msg_t service_msg;
    int rc = -1;

    os_memset(&service_msg, 0x0, sizeof(bt_pan_service_msg_t));

    if (bt_pan_service_msg_que == NULL)
    {
        return;
    }

    uint16_t data_len = sizeof(eth_data_t) + data->payload_len;

    service_msg.data = (char *)os_malloc(data_len);

    if (service_msg.data == NULL)
    {
        LOGE("%s, malloc failed\r\n", __func__);
        return;
    }

    os_memcpy(service_msg.data, data, data_len);

    service_msg.type = BT_PAN_DATA_IND_MSG;
    service_msg.len = data_len;

    rc = rtos_push_to_queue(&bt_pan_service_msg_que, &service_msg, BEKEN_NO_WAIT);

    if (kNoErr != rc)
    {
        LOGE("%s, send queue failed\r\n", __func__);

        if (service_msg.data)
        {
            os_free(service_msg.data);
        }
    }
}

void bt_pan_tx_done_ind(void)
{
    bt_pan_service_msg_t service_msg;
    int rc = -1;

    os_memset(&service_msg, 0x0, sizeof(bt_pan_service_msg_t));

    if (bt_pan_service_msg_que == NULL)
    {
        return;
    }

    service_msg.type = BT_PAN_TX_DONE_IND_MSG;
    service_msg.len = 0;

    rc = rtos_push_to_queue(&bt_pan_service_msg_que, &service_msg, BEKEN_NO_WAIT);

    if (kNoErr != rc)
    {
        LOGE("%s, send queue failed\r\n", __func__);
    }
}

void bt_pan_trigger_tx(void)
{
    bt_pan_service_msg_t service_msg;
    int rc = -1;

    os_memset(&service_msg, 0x0, sizeof(bt_pan_service_msg_t));

    if (bt_pan_service_msg_que == NULL)
    {
        return;
    }

    service_msg.type = BT_PAN_TX_MSG;
    service_msg.len = 0;

    rc = rtos_push_to_queue(&bt_pan_service_msg_que, &service_msg, BEKEN_NO_WAIT);

    if (kNoErr != rc)
    {
        LOGE("%s, send queue failed\r\n", __func__);
    }
}

static void bk_bt_app_pan_cb(bk_pan_cb_event_t event, bk_pan_cb_param_t *param)
{
    LOGD("%s event: %d\r\n", __func__, event);

    switch (event)
    {
        case BK_PAN_CONNECTION_STATE_EVT:
        {
            uint8_t *bda = param->conn_state.remote_bda;
            LOGI("PAN connection state: %d, [%02x:%02x:%02x:%02x:%02x:%02x]\r\n",
                 param->conn_state.con_state, bda[5], bda[4], bda[3], bda[2], bda[1], bda[0]);

            s_pan_state = param->conn_state.con_state;
            if (BK_BTPAN_STATE_CONNECTED == param->conn_state.con_state)
            {
                os_memcpy(paired_bt_mac, bda, 6);
                bt_manager_set_connect_state(BT_STATE_PROFILE_CONNECTED);
#if 0
                np_type_filter_t np_type;
                np_type.num_filters = 2;
                np_type.start[0] =  0x800;
                np_type.end[0] =  0x800;
                np_type.start[1] =  0x806;
                np_type.end[1] =  0x806;
                bk_bt_pan_set_protocol_filters(bda, &np_type);
#endif
#if CONFIG_NET_PAN
                pan_ip_start();
#endif
            }
            else
            {
#if CONFIG_NET_PAN
                LOGD("PAN not connected!\r\n");
                pan_ip_down();
#endif
                if (s_pan_tx_list)
                {
                    rtos_lock_mutex(&s_pan_tx_lock);
                    bt_comm_list_clear(s_pan_tx_list);
                    rtos_unlock_mutex(&s_pan_tx_lock);
                    s_acl_buf_count = CONFIG_NB_ACL_BUFF;
                }
            }
        }
        break;

        case BK_PAN_DATA_READY_EVT:
        {
            bt_pan_media_data_ind(param->pan_data.eth_data);
        }
        break;

        case BK_PAN_WRITE_DATA_CNF_EVT:
        {
            LOGD("PAN WRITE DATA CNF\r\n");
            PAN_TX_DONE();
            bt_pan_tx_done_ind();
        }
        break;

        default:
            LOGW("Invalid PAN event: %d\r\n", event);
            break;
    }
}

static void bk_pan_connect(uint8_t *remote_addr)
{
    bk_bt_pan_connect(remote_addr, BK_PAN_ROLE_PANU, BK_PAN_ROLE_NAP);
}

static void bk_pan_disconnect(uint8_t *remote_addr)
{
    bk_bt_pan_disconnect(remote_addr);
}

#if CONFIG_NET_PAN
static void pan_output(struct netif *netif, struct pbuf *p)
{
    struct eth_hdr *ethhdr;

    ethhdr = (struct eth_hdr *)p->payload;

    PAN_NET_OUTPUT();

    eth_data_t *p_eth_data = os_malloc(sizeof(eth_data_t) + p->tot_len);
    if (p_eth_data == NULL)
    {
        return;
    }

    os_memcpy(p_eth_data->dest, ethhdr->dest.addr, PAN_HWADDR_LEN);
    os_memcpy(p_eth_data->src, ethhdr->src.addr, PAN_HWADDR_LEN);

    LOGD("src_mac %2x:%2x:%2x:%2x:%2x:%2x\r\n", p_eth_data->src[0], p_eth_data->src[1], p_eth_data->src[2],
         p_eth_data->src[3], p_eth_data->src[4], p_eth_data->src[5]);

    LOGD("dest_mac %2x:%2x:%2x:%2x:%2x:%2x\r\n", p_eth_data->dest[0], p_eth_data->dest[1], p_eth_data->dest[2],
         p_eth_data->dest[3], p_eth_data->dest[4], p_eth_data->dest[5]);

#ifdef CONFIG_IPV6
    p_eth_data->protocol = 0x86dd;
#endif

    p_eth_data->protocol = htons(ethhdr->type);
    p_eth_data->payload_len = p->tot_len;
    os_memcpy(p_eth_data->payload, (uint8_t *)p->payload + sizeof(struct eth_hdr), p_eth_data->payload_len);

    //LOGI("data %d\r\n",p->tot_len);

    if (BK_BTPAN_STATE_CONNECTED == s_pan_state)
    {
        rtos_lock_mutex(&s_pan_tx_lock);
        pan_push_tx_data_to_list(p_eth_data, sizeof(eth_data_t) + p->tot_len);
        rtos_unlock_mutex(&s_pan_tx_lock);
        bt_pan_trigger_tx();
    }
    else
    {
        LOGE("%s, pan is not connected!\r\n", __func__);
    }

    os_free(p_eth_data);

}
#endif

int pan_service_init(void)
{
    LOGI("%s\r\n", __func__);

    if (s_pan_service_already_init)
    {
        LOGI("pan service already initialised\r\n");
        return 0;
    }

    bt_manager_init(0);

    btm_callback_s btm_cb =
    {
        .gap_cb = NULL,
        .start_connect_cb = bk_pan_connect,
        .start_disconnect_cb = bk_pan_disconnect,
        .stop_connect_cb = NULL,
    };
    bt_manager_register_callback(&btm_cb);

    bt_pan_service_task_init();

    bk_bt_pan_init(BK_PAN_ROLE_PANU);

    bk_bt_pan_register_callback(bk_bt_app_pan_cb);
#if CONFIG_NET_PAN
    net_pan_init();
    bk_panif_register_callback(pan_output);
#endif
    cli_pan_demo_init();

    /***for iphone***/
    bt_hidd_init();
    /***for iphone***/

    s_pan_tx_list = bt_comm_list_new();
    if (!s_pan_tx_list)
    {
        LOGE("%s, s_pan_tx_list failed to new!\r\n", __func__);
    }
    rtos_init_mutex(&s_pan_tx_lock);

    s_acl_buf_count = CONFIG_NB_ACL_BUFF;

    s_pan_service_already_init = 1;
    return 0;
}

void bt_pan_service_task_deinit(void)
{
    bt_pan_service_msg_t service_msg;
    int rc = -1;

    os_memset(&service_msg, 0x0, sizeof(bt_pan_service_msg_t));

    if (bt_pan_service_msg_que == NULL)
    {
        return;
    }

    service_msg.type = BT_PAN_EXIT_MSG;
    service_msg.len = 0;

    rc = rtos_push_to_queue(&bt_pan_service_msg_que, &service_msg, BEKEN_NO_WAIT);

    if (kNoErr != rc)
    {
        LOGE("%s, send queue failed\r\n", __func__);
    }
}


int pan_service_deinit(void)
{
    LOGI("%s\r\n", __func__);

    if (!s_pan_service_already_init)
    {
        LOGI("pan service already de-initialised\r\n");
        return 0;
    }

    /***for iphone***/
    bt_hidd_deinit();
    /***for iphone***/

    bt_manager_deinit();

    bt_pan_service_task_deinit();

    pan_ip_down();
    net_pan_remove_netif();

    cli_pan_demo_deinit();

    if (s_pan_tx_list)
    {
        rtos_lock_mutex(&s_pan_tx_lock);
        bt_comm_list_free(s_pan_tx_list);
        rtos_unlock_mutex(&s_pan_tx_lock);
        s_pan_tx_list = NULL;
    }

    if (s_pan_tx_lock)
    {
        rtos_deinit_mutex(&s_pan_tx_lock);
        s_pan_tx_lock = NULL;
    }

    s_pan_service_already_init = 0;
    return 0;
}

