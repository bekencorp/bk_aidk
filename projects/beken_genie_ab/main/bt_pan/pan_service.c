#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "pan_service.h"

#include "components/bluetooth/bk_dm_bluetooth_types.h"
#include "components/bluetooth/bk_dm_bt_types.h"
#include "components/bluetooth/bk_dm_bt.h"
#include "components/bluetooth/bk_dm_pan.h"
#include "storage/bluetooth_storage.h"
#include "pan_user_config.h"
#include "bt_manager.h"

#define TAG "pan"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

enum
{
    BT_PAN_MSG_NULL = 0,
    BT_PAN_DATA_IND_MSG = 1,
};

typedef struct
{
    uint8_t type;
    uint16_t len;
    char *data;
} bt_pan_service_msg_t;

static beken_queue_t bt_pan_service_msg_que = NULL;
static beken_thread_t bt_pan_service_thread_handle = NULL;

void bt_pan_service_main(void *arg)
{
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

                    uint8_t *dest = p_data->dest;
                    uint8_t *src = p_data->src;
                    LOGI("PAN_DATA_IND, pro:0x%04x, dest[%02x:%02x:%02x:%02x:%02x:%02x],src[%02x:%02x:%02x:%02x:%02x:%02x], len : %d\r\n",
                         p_data->protocol, dest[0], dest[1], dest[2], dest[3], dest[4], dest[5],
                         src[0], src[1], src[2], src[3], src[4], src[5], p_data->payload_len);
                    LOGI(" data : %x %x- %x %x\r\n", p_data->payload[0], p_data->payload[1], p_data->payload[p_data->payload_len - 2], p_data->payload[p_data->payload_len - 1]);

                    os_free(msg.data);
                }
                break;

                default:
                    break;
            }
        }
    }

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

static void bk_bt_app_pan_cb(bk_pan_cb_event_t event, bk_pan_cb_param_t *param)
{
    LOGI("%s event: %d\r\n", __func__, event);

    switch (event)
    {
        case BK_PAN_CONNECTION_STATE_EVT:
        {
            uint8_t *bda = param->conn_state.remote_bda;
            LOGI("PAN connection state: %d, [%02x:%02x:%02x:%02x:%02x:%02x]\r\n",
                 param->conn_state.con_state, bda[5], bda[4], bda[3], bda[2], bda[1], bda[0]);

            if (BK_BTPAN_STATE_CONNECTED == param->conn_state.con_state)
            {
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
            LOGI("PAN WRITE DATA CNF\r\n");
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

int pan_service_init(void)
{
    LOGI("%s\r\n", __func__);

    bt_manager_init();

    btm_callback_s btm_cb =
    {
        .gap_cb = NULL,
        .start_connect_cb = bk_pan_connect,
        .start_disconnect_cb = NULL,
        .stop_connect_cb = NULL,
    };
    bt_manager_register_callback(&btm_cb);

    bt_pan_service_task_init();

    bk_bt_pan_init(BK_PAN_ROLE_PANU);

    bk_bt_pan_register_callback(bk_bt_app_pan_cb);

    cli_pan_demo_init();
    return 0;
}

