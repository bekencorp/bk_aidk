#include <common/sys_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <components/log.h>
#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>

#include "components/bluetooth/bk_dm_bluetooth.h"
#include "components/bluetooth/bk_dm_bluetooth_types.h"
#include "components/bluetooth/bk_dm_gap_ble_types.h"
#include "components/bluetooth/bk_dm_gap_ble.h"
#include "components/bluetooth/bk_dm_gatt_types.h"
#include "components/bluetooth/bk_dm_gatts.h"

#include "wifi_boarding_internal.h"
#include "wifi_boarding_utils.h"


static ble_boarding_info_t *s_ble_boarding_info = NULL;
static beken_semaphore_t s_ble_sema = NULL;
static bk_gatt_if_t s_gatts_if = 0;
extern bool enable_ble_split_pkt;

#define SYNC_CMD_TIMEOUT_MS 4000
#define ADV_HANDLE 0


#define BK_GATT_ATTR_TYPE(iuuid) {.len = BK_UUID_LEN_16, .uuid = {.uuid16 = iuuid}}
#define BK_GATT_ATTR_CONTENT(iuuid) {.len = BK_UUID_LEN_16, .uuid = {.uuid16 = iuuid}}
#define BK_GATT_ATTR_VALUE(ilen, ivalue) {.attr_max_len = ilen, .attr_len = ilen, .attr_value = ivalue}

#define BK_GATT_ATTR_TYPE_128(iuuid) {.len = BK_UUID_LEN_128, .uuid = {.uuid128 = {iuuid[0], iuuid[1], iuuid[2], iuuid[3], iuuid[4], \
                                                                                       iuuid[5], iuuid[6], iuuid[7], iuuid[8], iuuid[9], iuuid[10], iuuid[11], iuuid[12], iuuid[13], iuuid[14], iuuid[15]}}}

#define BK_GATT_ATTR_CONTENT_128(iuuid) {.len = BK_UUID_LEN_128, .uuid = {.uuid128 = {iuuid[0], iuuid[1], iuuid[2], iuuid[3], iuuid[4], \
                                                                                          iuuid[5], iuuid[6], iuuid[7], iuuid[8], iuuid[9], iuuid[10], iuuid[11], iuuid[12], iuuid[13], iuuid[14], iuuid[15]}}}

#define BK_GATT_PRIMARY_SERVICE_DECL(iuuid) \
    .att_desc =\
               {\
                .attr_type = BK_GATT_ATTR_TYPE(BK_GATT_UUID_PRI_SERVICE),\
                .attr_content = BK_GATT_ATTR_CONTENT(iuuid),\
               }

#define BK_GATT_PRIMARY_SERVICE_DECL_128(iuuid) \
    .att_desc =\
               {\
                .attr_type = BK_GATT_ATTR_TYPE(BK_GATT_UUID_PRI_SERVICE),\
                .attr_content = BK_GATT_ATTR_CONTENT_128(iuuid)\
               }

#define BK_GATT_CHAR_DECL(iuuid, ilen, ivalue, iprop, iperm, irsp) \
    .att_desc = \
                {\
                 .attr_type = BK_GATT_ATTR_TYPE(BK_GATT_UUID_CHAR_DECLARE),\
                 .attr_content = BK_GATT_ATTR_CONTENT(iuuid),\
                 .value = BK_GATT_ATTR_VALUE(ilen, ivalue),\
                 .prop = iprop,\
                 .perm = iperm,\
                },\
                .attr_control = {.auto_rsp = irsp}

#define BK_GATT_CHAR_DECL_128(iuuid, ilen, ivalue, iprop, iperm, irsp) \
    .att_desc = \
                {\
                 .attr_type = BK_GATT_ATTR_TYPE(BK_GATT_UUID_CHAR_DECLARE),\
                 .attr_content = BK_GATT_ATTR_CONTENT_128(iuuid),\
                 .value = BK_GATT_ATTR_VALUE(ilen, ivalue),\
                 .prop = iprop,\
                 .perm = iperm,\
                },\
                .attr_control = {.auto_rsp = irsp}

#define BK_GATT_CHAR_DESC_DECL(iuuid, ilen, ivalue, iperm, irsp) \
    .att_desc = \
                {\
                 .attr_type = BK_GATT_ATTR_TYPE(iuuid),\
                 .value = BK_GATT_ATTR_VALUE(ilen, ivalue),\
                 .perm = iperm,\
                },\
                .attr_control = {.auto_rsp = irsp}

#define BK_GATT_CHAR_DESC_DECL_128(iuuid, ilen, ivalue, iperm, irsp) \
    .att_desc = \
                {\
                 .attr_type = BK_GATT_ATTR_TYPE_128(iuuid),\
                 .value = BK_GATT_ATTR_VALUE(ilen, ivalue),\
                 .perm = iperm,\
                },\
                .attr_control = {.auto_rsp = irsp}

#define INVALID_ATTR_HANDLE 0

static uint16_t s_prop_cli_config;
static uint8_t s_ssid[64];
static uint8_t s_password[64];
static uint16_t s_conn_ind = ~0;


static const bk_gatts_attr_db_t s_gatts_attr_db_service_boarding[] =
{
    {
        BK_GATT_PRIMARY_SERVICE_DECL(0xfa00),
    },

    {
        BK_GATT_CHAR_DECL(0xea01,
                          0, NULL,
                          BK_GATT_CHAR_PROP_BIT_NOTIFY,
                          BK_GATT_PERM_READ,
                          BK_GATT_RSP_BY_APP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_CHAR_CLIENT_CONFIG,
                               sizeof(s_prop_cli_config), (uint8_t *)&s_prop_cli_config,
                               BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                               BK_GATT_RSP_BY_APP),
    },

    //operation
    {
        BK_GATT_CHAR_DECL(0xea02,
                          0, NULL,
                          BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_WRITE,
                          BK_GATT_RSP_BY_APP),
    },

    //ssid
    {
        BK_GATT_CHAR_DECL(0xea05,
                          sizeof(s_password), (uint8_t *)s_password,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },

    //password
    {
        BK_GATT_CHAR_DECL(0xea06,
                          sizeof(s_ssid), (uint8_t *)s_ssid,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },
};

static uint16_t s_service_attr_handle = INVALID_ATTR_HANDLE;
static uint16_t s_char_attr_handle = INVALID_ATTR_HANDLE;
static uint16_t s_char_desc_attr_handle = INVALID_ATTR_HANDLE;
static uint16_t s_char_operation_char_handle = INVALID_ATTR_HANDLE;
static uint16_t s_char_ssid_char_handle = INVALID_ATTR_HANDLE;
static uint16_t s_char_password_char_handle = INVALID_ATTR_HANDLE;


static uint16_t *const s_boarding_attr_handle_list[sizeof(s_gatts_attr_db_service_boarding) / sizeof(s_gatts_attr_db_service_boarding[0])] =
{
    &s_service_attr_handle,
    &s_char_attr_handle,
    &s_char_desc_attr_handle,
    &s_char_operation_char_handle,
    &s_char_ssid_char_handle,
    &s_char_password_char_handle,
};

static int32_t dm_gatts_get_buff_from_attr_handle(bk_gatts_attr_db_t *attr_list, uint16_t *attr_handle_list, uint32_t size, uint16_t attr_handle, uint32_t *output_index, uint8_t **output_buff, uint32_t *output_size)
{
    uint32_t i;

    for (i = 0; i < size; ++i)
    {
        if (attr_handle_list[i] == attr_handle)
        {
            break;
        }
    }

    if (i >= size)
    {
        return -1;
    }

    *output_index = i;
    *output_buff = attr_list[i].att_desc.value.attr_value;
    *output_size = attr_list[i].att_desc.value.attr_len;

    return 0;
}

static int32_t wifi_boarding_gatts_cb(bk_gatts_cb_event_t event, bk_gatt_if_t gatts_if, bk_ble_gatts_cb_param_t *comm_param)
{
    ble_err_t ret = 0;

    switch (event)
    {
    case BK_GATTS_REG_EVT:
    {
        struct gatts_reg_evt_param *param = (typeof(param))comm_param;

        wboard_logi("BK_GATTS_REG_EVT %d %d", param->status, param->gatt_if);
        s_gatts_if = param->gatt_if;

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    case BK_GATTS_UNREG_EVT:
    {
        wboard_logi("BK_GATTS_UNREG_EVT");

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_GATTS_START_EVT:
    {
        struct gatts_start_evt_param *param = (typeof(param))comm_param;
        wboard_logi("BK_GATTS_START_EVT compl %d %d", param->status, param->service_handle);

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    case BK_GATTS_STOP_EVT:
    {
        struct gatts_stop_evt_param *param = (typeof(param))comm_param;
        wboard_logi("BK_GATTS_STOP_EVT compl %d %d", param->status, param->service_handle);

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    case BK_GATTS_CREAT_ATTR_TAB_EVT:
    {
        struct gatts_add_attr_tab_evt_param *param = (typeof(param))comm_param;

        wboard_logi("BK_GATTS_CREAT_ATTR_TAB_EVT %d %d", param->status, param->num_handle);

        for (int i = 0; i < param->num_handle; ++i)
        {
            *s_boarding_attr_handle_list[i] = param->handles[i];
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    case BK_GATTS_READ_EVT:
    {
        struct gatts_read_evt_param *param = (typeof(param))comm_param;
        bk_gatt_rsp_t rsp;
        uint16_t final_len = 0;

        memset(&rsp, 0, sizeof(rsp));
        wboard_logi("read attr handle %d need rsp %d", param->handle, param->need_rsp);

        uint8_t *tmp_buff = NULL;
        uint16_t buff_size = 0;
        uint8_t valid = 1;

        if (s_char_desc_attr_handle == param->handle)
        {
            bk_ble_gatts_get_attr_value(param->handle, &buff_size, &tmp_buff);
        }
        else if (s_char_ssid_char_handle == param->handle)
        {
            bk_ble_gatts_get_attr_value(param->handle, &buff_size, &tmp_buff);
        }
        else if (s_char_password_char_handle == param->handle)
        {
            bk_ble_gatts_get_attr_value(param->handle, &buff_size, &tmp_buff);
        }
        else
        {
            wboard_loge("invalid read handle %d", param->handle);
            valid = 0;
        }

        if (param->need_rsp)
        {
            final_len = buff_size - param->offset;

            rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
            rsp.attr_value.handle = param->handle;
            rsp.attr_value.offset = param->offset;

            if (tmp_buff && valid)
            {
                rsp.attr_value.len = final_len;
                rsp.attr_value.value = tmp_buff + param->offset;
            }
            else
            {
                rsp.attr_value.len = 0;
                rsp.attr_value.value = NULL;
            }

            ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id,
                                             (tmp_buff && valid ? BK_GATT_OK : BK_GATT_INSUF_RESOURCE), &rsp);
        }
    }
    break;

    case BK_GATTS_WRITE_EVT:
    {
        struct gatts_write_evt_param *param = (typeof(param))comm_param;
        bk_gatt_rsp_t rsp;
        uint16_t final_len = 0;

        memset(&rsp, 0, sizeof(rsp));

        wboard_logi("write attr handle %d len %d offset %d need rsp %d", param->handle, param->len, param->offset, param->need_rsp);

        uint8_t *tmp_buff = NULL;
        uint16_t buff_size = 0;
        uint8_t valid = 1;

        if (s_char_desc_attr_handle == param->handle)
        {
            bk_ble_gatts_get_attr_value(param->handle, &buff_size, &tmp_buff);
        }
        else if (s_char_operation_char_handle == param->handle)
        {
            bk_ble_gatts_get_attr_value(param->handle, &buff_size, &tmp_buff);
            wboard_logi("write boarding op char");
        }
        else if (s_char_ssid_char_handle == param->handle)
        {
            bk_ble_gatts_get_attr_value(param->handle, &buff_size, &tmp_buff);

            if (s_ble_boarding_info->ssid_value)
            {
                os_free(s_ble_boarding_info->ssid_value);
                s_ble_boarding_info->ssid_value = NULL;
                s_ble_boarding_info->ssid_length = 0;
            }

            s_ble_boarding_info->ssid_length = param->len;
            s_ble_boarding_info->ssid_value = os_malloc(param->len + 1);

            if (!s_ble_boarding_info->ssid_value)
            {
                wboard_loge("alloc ssid err");
                valid = 0;
            }
            else
            {
                os_memset(s_ble_boarding_info->ssid_value, 0, param->len + 1);
                os_memcpy((uint8_t *)s_ble_boarding_info->ssid_value, param->value, param->len);

                wboard_logi("ssid: %s", s_ble_boarding_info->ssid_value);
            }
        }
        else if (s_char_password_char_handle == param->handle)
        {
            bk_ble_gatts_get_attr_value(param->handle, &buff_size, &tmp_buff);

            if (s_ble_boarding_info->password_value)
            {
                os_free(s_ble_boarding_info->password_value);
                s_ble_boarding_info->password_value = NULL;
                s_ble_boarding_info->password_length = 0;
            }

            s_ble_boarding_info->password_length = param->len;
            s_ble_boarding_info->password_value = os_malloc(param->len + 1);

            if (!s_ble_boarding_info->password_value)
            {
                wboard_loge("alloc password err");
                valid = 0;
            }
            else
            {
                os_memset(s_ble_boarding_info->password_value, 0, param->len + 1);
                os_memcpy((uint8_t *)s_ble_boarding_info->password_value, param->value, param->len);
                wboard_logi("password: %s", s_ble_boarding_info->password_value);
            }
        }
        else
        {
            wboard_loge("invalid write handle %d", param->handle);
            valid = 0;
        }

        if (param->need_rsp)
        {
            final_len = (param->len < buff_size - param->offset ? param->len :  buff_size - param->offset);

            if (tmp_buff)
            {
                os_memcpy(tmp_buff + param->offset, param->value, final_len);
            }

            rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
            rsp.attr_value.handle = param->handle;
            rsp.attr_value.offset = param->offset;

            if (tmp_buff && valid)
            {
                rsp.attr_value.len = final_len;
                rsp.attr_value.value = tmp_buff + param->offset;
            }

            ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, valid ? BK_GATT_OK : BK_GATT_INSUF_RESOURCE, &rsp);
        }

        if (s_char_operation_char_handle == param->handle)
        {
            uint16_t opcode = 0;
            uint16_t length = 0;
            uint8_t *data = NULL;

            if (param->len < 2)
            {
                wboard_loge("len invalid %d", param->len);
                break;
            }

            opcode = param->value[0] | param->value[1] << 8;

            if (param->len >= 4)
            {
                length = param->value[2] | param->value[3] << 8;
            }

            if (param->len > 4)
            {
                data = &param->value[4];
            }

            if (s_ble_boarding_info && s_ble_boarding_info->cb)
            {
                s_ble_boarding_info->cb(opcode, length, data);
            }
            else
            {
                wboard_loge("invalid s_ble_boarding_info");
                break;
            }

#if 0
            uint8_t test_data[20] = {0};
            uint16_t test_data_len = sizeof(test_data) - 2 - 1 - 2;
            os_memcpy(test_data, &opcode, sizeof(opcode));
            test_data[2] = 0;
            os_memcpy(test_data + 3, &test_data_len, sizeof(test_data_len));

            wifi_boarding_notify(test_data, sizeof(test_data));
#endif
        }


    }
    break;

    case BK_GATTS_EXEC_WRITE_EVT:
    {
        struct gatts_exec_write_evt_param *param = (typeof(param))comm_param;
        wboard_logi("exec write");
    }
    break;

    case BK_GATTS_CONF_EVT:
    {
        struct gatts_conf_evt_param *param = (typeof(param))comm_param;

        wboard_logi("BK_GATTS_CONF_EVT %d %d %d", param->status, param->conn_id, param->handle);
    }
    break;

    case BK_GATTS_RESPONSE_EVT:
    {
        struct gatts_rsp_evt_param *param = (typeof(param))comm_param;

        wboard_logi("BK_GATTS_RESPONSE_EVT %d %d", param->status, param->handle);
    }
    break;

    case BK_GATTS_SEND_SERVICE_CHANGE_EVT:
    {
        struct gatts_send_service_change_evt_param *param = (typeof(param))comm_param;

        wboard_logi("BK_GATTS_SEND_SERVICE_CHANGE_EVT %02x:%02x:%02x:%02x:%02x:%02x %d %d",
                    param->remote_bda[5],
                    param->remote_bda[4],
                    param->remote_bda[3],
                    param->remote_bda[2],
                    param->remote_bda[1],
                    param->remote_bda[0],
                    param->status, param->conn_id);
    }
    break;

    case BK_GATTS_CONNECT_EVT:
    {
        struct gatts_connect_evt_param *param = (typeof(param))comm_param;

        wboard_logi("BK_GATTS_CONNECT_EVT role %d %02X:%02X:%02X:%02X:%02X:%02X conn_id %d ",
                    param->link_role,
                    param->remote_bda[5],
                    param->remote_bda[4],
                    param->remote_bda[3],
                    param->remote_bda[2],
                    param->remote_bda[1],
                    param->remote_bda[0],
                    param->conn_id);

        s_conn_ind = param->conn_id;
    }
    break;

    case BK_GATTS_DISCONNECT_EVT:
    {
        struct gatts_disconnect_evt_param *param = (typeof(param))comm_param;

        wboard_logi("BK_GATTS_DISCONNECT_EVT %02X:%02X:%02X:%02X:%02X:%02X conn_id %d",
                    param->remote_bda[5],
                    param->remote_bda[4],
                    param->remote_bda[3],
                    param->remote_bda[2],
                    param->remote_bda[1],
                    param->remote_bda[0],
                    param->conn_id
                   );

        s_conn_ind = ~0;
    }
    break;

    case BK_GATTS_MTU_EVT:
    {
        struct gatts_mtu_evt_param *param = (typeof(param))comm_param;

        wboard_logi("BK_GATTS_MTU_EVT %d %d", param->conn_id, param->mtu);
    }
    break;

    default:
        break;
    }

    return ret;
}


static void dm_ble_gap_common_cb(bk_ble_gap_cb_event_t event, bk_ble_gap_cb_param_t *param)
{
    wboard_logd("event %d", event);

    switch (event)
    {
    case BK_BLE_GAP_CONNECT_COMPLETE_EVT:
    {
        struct ble_connect_complete_param *evt = (typeof(evt))param;

        wboard_logi("BK_BLE_GAP_CONNECT_COMPLETE_EVT %02x:%02x:%02x:%02x:%02x:%02x status 0x%x role %d hci_handle 0x%x",
                    evt->remote_bda[5],
                    evt->remote_bda[4],
                    evt->remote_bda[3],
                    evt->remote_bda[2],
                    evt->remote_bda[1],
                    evt->remote_bda[0],
                    evt->status,
                    evt->link_role,
                    evt->hci_handle
                   );
    }
    break;

    case BK_BLE_GAP_DISCONNECT_COMPLETE_EVT:
    {
        struct ble_disconnect_complete_param *evt = (typeof(evt))param;

        wboard_logi("BK_BLE_GAP_DISCONNECT_COMPLETE_EVT %02x:%02x:%02x:%02x:%02x:%02x %d status 0x%x reason 0x%x hci_handle 0x%x",
                    evt->remote_bda[5],
                    evt->remote_bda[4],
                    evt->remote_bda[3],
                    evt->remote_bda[2],
                    evt->remote_bda[1],
                    evt->remote_bda[0],
                    evt->remote_bda_type,
                    evt->status,
                    evt->reason,
                    evt->hci_handle
                   );

    }
    break;

    case BK_BLE_GAP_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT:
    {
        struct ble_adv_set_rand_addr_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            wboard_loge("set adv rand addr err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_PARAMS_SET_COMPLETE_EVT:
    {
        struct ble_adv_params_set_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            wboard_loge("set adv param err 0x%x", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_DATA_SET_COMPLETE_EVT:
    {
        struct ble_adv_data_set_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            wboard_loge("set adv data err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_DATA_RAW_SET_COMPLETE_EVT:
    {
        struct ble_adv_data_raw_set_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            wboard_loge("set raw adv data err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_START_COMPLETE_EVT:
    {
        struct ble_adv_start_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            wboard_loge("set adv enable err %d", pm->status);
        }

        wboard_logi("pls disable adv before remove pair !!!");

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_STOP_COMPLETE_EVT:
    {
        struct ble_adv_stop_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            wboard_loge("set adv disable err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_SET_REMOVE_COMPLETE_EVT:
    {
        struct ble_adv_set_remove_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            wboard_loge("remove adv set err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore(&s_ble_sema);
        }
    }
    break;

    default:
        break;
    }

}

#if CONFIG_BT//dm
int wifi_boarding_init(ble_boarding_info_t *info)
{
    bt_err_t ret = BK_FAIL;

    s_ble_boarding_info = info;
    enable_ble_split_pkt = false;
    if(!s_ble_sema)
    {
        ret = rtos_init_semaphore(&s_ble_sema, 1);

        if (ret != 0)
        {
            wboard_loge("rtos_init_semaphore err %d", ret);
            return -1;
        }
    }

    bk_ble_gap_register_callback(dm_ble_gap_common_cb);

    bk_ble_gatts_register_callback(wifi_boarding_gatts_cb);

    ret = bk_ble_gatts_app_register(0);

    if (ret)
    {
        wboard_loge("reg err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("rtos_get_semaphore reg err %d", ret);
        return -1;
    }

    bk_ble_gatts_create_attr_tab(s_gatts_attr_db_service_boarding, s_gatts_if, sizeof(s_gatts_attr_db_service_boarding) / sizeof(s_gatts_attr_db_service_boarding[0]), 30);

    if (ret != 0)
    {
        wboard_loge("bk_ble_gatts_create_attr_tab err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

    bk_ble_gatts_start_service(s_service_attr_handle);

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

    return BK_OK;
}

int wifi_boarding_deinit(void)
{
    int32_t ret = 0;

    wboard_logw("");

    if (s_ble_boarding_info->ssid_value)
    {
        os_free(s_ble_boarding_info->ssid_value);
    }

    if (s_ble_boarding_info->password_value)
    {
        os_free(s_ble_boarding_info->password_value);
    }


    ret = bk_ble_gatts_app_unregister(s_gatts_if);

    if (ret)
    {
        wboard_loge("unreg err %d", ret);
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("rtos_get_semaphore unreg err %d", ret);
    }

    os_memset(s_ble_boarding_info, 0, sizeof(*s_ble_boarding_info));

    if (s_ble_sema)
    {
        ret = rtos_deinit_semaphore(&s_ble_sema);

        if (ret != 0)
        {
            wboard_loge("rtos_deinit_semaphore err %d", ret);
            return -1;
        }

        s_ble_sema = NULL;
    }

    bk_ble_gatts_register_callback(NULL);

    s_gatts_if = 0;
    s_prop_cli_config = 0;
    os_memset(s_ssid, 0, sizeof(s_ssid));
    os_memset(s_password, 0, sizeof(s_password));
    s_conn_ind = ~0;
    s_service_attr_handle = INVALID_ATTR_HANDLE;
    s_char_attr_handle = INVALID_ATTR_HANDLE;
    s_char_desc_attr_handle = INVALID_ATTR_HANDLE;
    s_char_operation_char_handle = INVALID_ATTR_HANDLE;
    s_char_ssid_char_handle = INVALID_ATTR_HANDLE;
    s_char_password_char_handle = INVALID_ATTR_HANDLE;

    return BK_OK;
}

void dm_ble_gap_get_identity_addr(uint8_t *addr)
{
    uint8_t *identity_addr = addr;
    bk_get_mac((uint8_t *)identity_addr, MAC_TYPE_BLUETOOTH);

    for (int i = 0; i < BK_BD_ADDR_LEN / 2; i++)
    {
        uint8_t tmp = identity_addr[i];
        identity_addr[i] = identity_addr[BK_BD_ADDR_LEN - 1 - i];
        identity_addr[BK_BD_ADDR_LEN - 1 - i] = tmp;
    }
}

int wifi_boarding_adv_start(void)
{
    bt_err_t ret = BK_FAIL;

    bk_bd_addr_t current_addr = {0}, identity_addr = {0};
    char adv_name[64] = {0};

    dm_ble_gap_get_identity_addr(identity_addr);

    os_memcpy(current_addr, identity_addr, sizeof(identity_addr));

    current_addr[5] |= 0xc0;
    current_addr[0]++;

    snprintf((char *)(adv_name), sizeof(adv_name) - 1, "bk_genie-%02X%02X%02X", current_addr[2], current_addr[1], current_addr[0]);

    wboard_logi("adv name %s", adv_name);

    ret = bk_ble_gap_set_device_name(adv_name);

    if (ret)
    {
        wboard_loge("bk_ble_gap_set_device_name err %d", ret);
        goto error;
    }

    bk_ble_gap_ext_adv_params_t adv_param =
    {
        .type = BK_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND,
        .interval_min = 120 * 1,
        .interval_max = 160 * 1,
        .channel_map = BK_ADV_CHNL_ALL,
        .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        .primary_phy = BK_BLE_GAP_PRI_PHY_1M,
        .secondary_phy = BK_BLE_GAP_PHY_1M,
        .sid = 0,
        .scan_req_notif = 0,
        .own_addr_type = BLE_ADDR_TYPE_RANDOM,//BLE_ADDR_TYPE_PUBLIC,
    };

    ret =  bk_ble_gap_set_adv_params(ADV_HANDLE, &adv_param);

    if (ret != kNoErr)
    {
        wboard_loge("set adv param err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("wait set adv param err %d", ret);
        goto error;
    }

    ret = bk_ble_gap_set_adv_rand_addr(ADV_HANDLE, current_addr);

    if (ret)
    {
        wboard_loge("bk_ble_gap_set_adv_rand_addr err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("wait set adv rand addr err %d", ret);
        goto error;
    }

#define BEKEN_COMPANY_ID                    (0x05F0)
#define BOARDING_UUID                       (0xFE01)

#if 0
    const uint8_t baording_service_uuid[16] =
    {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00,
        BOARDING_UUID & 0xff, (BOARDING_UUID >> 8) & 0xff, 0x00, 0x00
    };

    const uint8_t manuf_data[] = {BEKEN_COMPANY_ID & 0xFF, BEKEN_COMPANY_ID >> 8};

    bk_ble_adv_data_t adv_data =
    {
        .set_scan_rsp = 0,
        .include_name = 1,
        .min_interval = 0x0006,
        .max_interval = 0x0010,
        .appearance = 0,
        .manufacturer_len = sizeof(manuf_data),
        .p_manufacturer_data = (void *)manuf_data,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(baording_service_uuid),
        .p_service_uuid = (void *)baording_service_uuid,
        .flag = 0x06,
    };

    ret = bk_ble_gap_set_adv_data((bk_ble_adv_data_t *)&adv_data);

    if (ret)
    {
        wboard_loge("bk_ble_gap_set_adv_data err %d", ret);
        goto error;
    }

#else
    uint8_t adv_data[251] = {0};
    uint32_t adv_index = 0, len_index = 0;

    /* flags */
    len_index = adv_index;
    adv_data[adv_index++] = 0x00;
    adv_data[adv_index++] = BK_BLE_AD_TYPE_FLAG;
    adv_data[adv_index++] = 0x06;
    adv_data[len_index] = 2;

    // name
    len_index = adv_index;
    adv_data[adv_index++] = 0x00;
    adv_data[adv_index++] = BK_BLE_AD_TYPE_NAME_CMPL;

    ret = sprintf((char *)&adv_data[adv_index], "%s", adv_name);

    adv_index += ret;
    adv_data[len_index] = ret + 1;

    /* 16bit uuid */
    len_index = adv_index;
    adv_data[adv_index++] = 0x00;
    adv_data[adv_index++] = BK_BLE_AD_TYPE_SERVICE_DATA;
    adv_data[adv_index++] = BOARDING_UUID & 0xFF;
    adv_data[adv_index++] = BOARDING_UUID >> 8;
    adv_data[len_index] = 3;

    /* manufacturer */
    len_index = adv_index;
    adv_data[adv_index++] = 0x00;
    adv_data[adv_index++] = BK_BLE_AD_TYPE_MANU;
    adv_data[adv_index++] = BEKEN_COMPANY_ID & 0xFF;
    adv_data[adv_index++] = BEKEN_COMPANY_ID >> 8;
    adv_data[len_index] = 3;

    ret = bk_ble_gap_set_adv_data_raw(0, adv_index, (const uint8_t *)adv_data);

    if (ret)
    {
        wboard_loge("bk_ble_gap_set_adv_data_raw err %d", ret);
        goto error;
    }

#endif

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("wait set adv data err %d", ret);
        goto error;
    }

    const bk_ble_gap_ext_adv_t ext_adv =
    {
        .instance = 0,
        .duration = 0,
        .max_events = 0,
    };

    ret = bk_ble_gap_adv_start(1, &ext_adv);

    if (ret)
    {
        wboard_loge("bk_ble_gap_adv_start err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("wait set adv enable err %d", ret);
        goto error;
    }

error:
    return 0;

}

int wifi_boarding_adv_stop(void)
{
    int32_t ret = 0;

    if(bk_bluetooth_get_status() != BK_BLUETOOTH_STATUS_ENABLED)
    {
        wboard_loge("bluetooth not init !!!");
        return BK_FAIL;
    }

    wboard_logi("");

    const uint8_t ext_adv_inst[] = {ADV_HANDLE};
    ret = bk_ble_gap_adv_stop(sizeof(ext_adv_inst) / sizeof(ext_adv_inst[0]), ext_adv_inst);

    if (ret)
    {
        wboard_loge("bk_ble_gap_adv_stop err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("wait stop adv err %d", ret);
        return -1;
    }

    ret = bk_ble_gap_adv_set_remove(ADV_HANDLE);

    if (ret)
    {
        wboard_loge("bk_ble_gap_adv_set_remove err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        wboard_loge("wait remove adv err %d", ret);
        return -1;
    }

    return BK_OK;
}

int wifi_boarding_notify(uint8_t *data, uint16_t length)
{
    if (s_conn_ind == 0xFF)
    {
        wboard_loge("BLE is disconnected, can not send data !!!");
        return BK_FAIL;
    }
    else
    {
        wboard_logi("len %d", length);
        bk_ble_gatts_send_indicate(s_gatts_if, s_conn_ind, s_char_attr_handle, length, data, 0);
        return BK_OK;
    }
}

#else//ble
int ble_boarding_init(ble_boarding_info_t *info);
int ble_boarding_deinit(void);
int ble_boarding_adv_start(uint8_t *adv_data, uint16_t adv_len);
int ble_boarding_adv_stop(void);
int ble_boarding_notify(uint8_t *data, uint16_t length);

#define ADV_MAX_SIZE (251)
#define ADV_NAME_HEAD "bk_genie"

#define ADV_TYPE_FLAGS                      (0x01)
#define ADV_TYPE_LOCAL_NAME                 (0x09)
#define ADV_TYPE_SERVICE_UUIDS_16BIT        (0x14)
#define ADV_TYPE_SERVICE_DATA               (0x16)
#define ADV_TYPE_MANUFACTURER_SPECIFIC      (0xFF)

#define BEKEN_COMPANY_ID                    (0x05F0)

#define BOARDING_UUID                       (0xFE01)

int wifi_boarding_init(ble_boarding_info_t *info)
{
    bt_err_t ret = BK_FAIL;

    wboard_logi("%s\n", __func__);
    enable_ble_split_pkt = false;
    ret = ble_boarding_init(info);
    return ret;
}

int wifi_boarding_deinit()
{
    int32_t ret = 0;

    wboard_logw("");
    ret = ble_boarding_deinit();
    return ret;
}

int wifi_boarding_adv_start(void)
{
    uint8_t adv_data[ADV_MAX_SIZE] = {0};
    uint8_t adv_index = 0;
    uint8_t len_index = 0;
    uint8_t mac[6];
    int ret;


    wboard_logi("%s\n", __func__);

    /* flags */
    len_index = adv_index;
    adv_data[adv_index++] = 0x00;
    adv_data[adv_index++] = ADV_TYPE_FLAGS;
    adv_data[adv_index++] = 0x06;
    adv_data[len_index] = 2;

    /* local name */
    bk_bluetooth_get_address(mac);

    len_index = adv_index;
    adv_data[adv_index++] = 0x00;
    adv_data[adv_index++] = ADV_TYPE_LOCAL_NAME;

    ret = sprintf((char *)&adv_data[adv_index], "%s_%02X%02X%02X",
                  ADV_NAME_HEAD, mac[0], mac[1], mac[2]);

    adv_index += ret;
    adv_data[len_index] = ret + 1;

    /* 16bit uuid */
    len_index = adv_index;
    adv_data[adv_index++] = 0x00;
    adv_data[adv_index++] = ADV_TYPE_SERVICE_DATA;
    adv_data[adv_index++] = BOARDING_UUID & 0xFF;
    adv_data[adv_index++] = BOARDING_UUID >> 8;
    adv_data[len_index] = 3;

    /* manufacturer */
    len_index = adv_index;
    adv_data[adv_index++] = 0x00;
    adv_data[adv_index++] = ADV_TYPE_MANUFACTURER_SPECIFIC;
    adv_data[adv_index++] = BEKEN_COMPANY_ID & 0xFF;
    adv_data[adv_index++] = BEKEN_COMPANY_ID >> 8;
    adv_data[len_index] = 3;

    /*
    os_printf("adv data:\n");

    int i = 0;
    for (i = 0; i < adv_index; i++)
    {
        os_printf("%02X ", adv_data[i]);
    }

    os_printf("\n");
    */
    ble_boarding_adv_stop();

    ret = ble_boarding_adv_start(adv_data, adv_index);

    return ret;
}

int wifi_boarding_adv_stop(void)
{
    int32_t ret = 0;

    if(bk_bluetooth_get_status() != BK_BLUETOOTH_STATUS_ENABLED)
    {
        wboard_loge("bluetooth not init !!!");
        return BK_FAIL;
    }

    ret = ble_boarding_adv_stop();

    return ret;
}

int wifi_boarding_notify(uint8_t *data, uint16_t length)
{
    return ble_boarding_notify(data, length);
}

#endif
