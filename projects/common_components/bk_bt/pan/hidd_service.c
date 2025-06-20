#include "bk_cli.h"
#include <common/sys_config.h>
#include <common/bk_kernel_err.h>
#include <string.h>
#include <stdlib.h>
#include "cli.h"
#include "components/bluetooth/bk_dm_hidd.h"
#include "components/bluetooth/bk_dm_bt.h"
#include "components/bluetooth/bk_dm_gap_bt_types.h"
#include "components/bluetooth/bk_dm_gap_bt.h"
#include "components/bluetooth/bk_dm_gap_bt_types.h"
#include "components/bluetooth/bk_dm_pan.h"


#define BT_HIDD_CMD_CNT sizeof(s_hidd_commands)/sizeof(struct cli_command)
#define BT_HIDD_TAG "BT-HID"

#define BTHIDD_LOGI(...) BK_LOGI(BT_HIDD_TAG, ##__VA_ARGS__)
#define BTHIDD_LOGW(...) BK_LOGW(BT_HIDD_TAG, ##__VA_ARGS__)
#define BTHIDD_LOGE(...) BK_LOGE(BT_HIDD_TAG, ##__VA_ARGS__)
#define BTHIDD_LOGD(...) BK_LOGD(BT_HIDD_TAG, ##__VA_ARGS__)

#define CMD_RSP_SUCCEED               "HIDD BT RSP:OK\r\n"
#define CMD_RSP_ERROR                 "HIDD BT RSP:ERROR\r\n"

#define KEY_BOARD_REPORT_ID 1
#define MOUSE_REPORT_ID     2

#define HIDD_DEMO_CLASS_OF_DEVICE 0x5C0

typedef struct{
    bk_bd_addr_t addr;
    bk_hidd_protocol_mode_t protocol_mode;
}hdidd_demo_param_t;

extern uint8_t s_pan_state;

static uint8_t keyboard_report[9] = {KEY_BOARD_REPORT_ID, 0U,0U,0U,0U,0U,0U,0U,0U};
static uint8_t report_flag = 0;


static uint8_t hid_demo_output_report_1[] = { KEY_BOARD_REPORT_ID,0xFFU };
static uint8_t hid_demo_input_report_1[] = { KEY_BOARD_REPORT_ID,0U,1U,2U,3U,4U,5U,6U,7U };
static uint8_t hid_demo_input_report_2[] = { MOUSE_REPORT_ID,0U,1U,2U };
static uint8_t hid_demo_feature_report_1[] = { KEY_BOARD_REPORT_ID,0U,1U,0U };

static hdidd_demo_param_t hd_param = {0};

static char hidd_demo_report_desc[] = 
{
    /*     ^^^^^         ^^^^^
     * Adjust lengths if Report Descriptor is changed.
     */
    /* HID Report Descriptor Length = 121 Octets (0x79) */
    0x08U,                       /* Usage */
    0x22U, 0x25U, 0x75U,         /* Unknown (bTag: 0x02, bType: 0x00) */
    0x05U, 0x01U,                /* Usage Page (Generic Desktop Ctrls) */
    0x09U, 0x06U,                /* Usage (Keyboard) */
    0xA1U, 0x01U,                /* Collection (Application) */
    0x85U, KEY_BOARD_REPORT_ID,                /*   Report ID (1) */
    0x05U, 0x07U,                /*   Usage Page (Kbrd/Keypad) */
    0x19U, 0xE0U,                /*   Usage Minimum (0xE0) */
    0x29U, 0xE7U,                /*   Usage Maximum (0xE7) */
    0x15U, 0x00U,                /*   Logical Minimum (0) */
    0x25U, 0x01U,                /*   Logical Maximum (1) */
    0x75U, 0x01U,                /*   Report Size (1) */
    0x95U, 0x08U,                /*   Report Count (8) */
    0x81U, 0x02U,                /*   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) */
    0x95U, 0x01U,                /*   Report Count (1) */
    0x75U, 0x08U,                /*   Report Size (8) */
    0x81U, 0x03U,                /*   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) */
    0x95U, 0x05U,                /*   Report Count (5)      */
    0x75U, 0x01U,                /*   Report Size (1) */
    0x05U, 0x08U,                /*   Usage Page (LEDs) */
    0x19U, 0x01U,                /*   Usage Minimum (Num Lock) */
    0x29U, 0x05U,                /*   Usage Maximum (Kana) */
    0x91U, 0x02U,                /*   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile) */
    0x95U, 0x01U,                /*   Report Count (1) */
    0x75U, 0x03U,                /*   Report Size (3) */
    0x91U, 0x03U,                /*   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile) */
    0x95U, 0x06U,                /*   Report Count (6) */
    0x75U, 0x08U,                /*   Report Size (8) */
    0x15U, 0x00U,                /*   Logical Minimum (0) */
    0x25U, 0x65U,                /*   Logical Maximum (101) */
    0x05U, 0x07U,                /*   Usage Page (Kbrd/Keypad) */
    0x19U, 0x00U,                /*   Usage Minimum (0x00) */
    0x29U, 0x65U,                /*   Usage Maximum (0x65) */
    0x81U, 0x00U,                /*   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position) */
    0xC0U,                       /* End Collection */
    0x05U, 0x01U,                /* Usage Page (Generic Desktop Ctrls) */
    0x09U, 0x02U,                /* Usage (Mouse) */
    0xA1U, 0x01U,                /* Collection (Application) */
    0x85U, MOUSE_REPORT_ID,                /*   Report ID (2) */
    0x09U, 0x01U,                /*   Usage (Pointer) */
    0xA1U, 0x00U,                /*   Collection (Physical) */
    0x05U, 0x09U,                /*     Usage Page (Button) */
    0x19U, 0x01U,                /*     Usage Minimum (0x01) */
    0x29U, 0x03U,                /*     Usage Maximum (0x03) */
    0x15U, 0x00U,                /*     Logical Minimum (0) */
    0x25U, 0x01U,                /*     Logical Maximum (1) */
    0x95U, 0x03U,                /*     Report Count (3) */
    0x75U, 0x01U,                /*     Report Size (1) */
    0x81U, 0x02U,                /*     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) */
    0x95U, 0x01U,                /*     Report Count (1) */
    0x75U, 0x05U,                /*     Report Size (5) */
    0x81U, 0x03U,                /*     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) */
    0x05U, 0x01U,                /*     Usage Page (Generic Desktop Ctrls) */
    0x09U, 0x30U,                /*     Usage (X) */
    0x09U, 0x31U,                /*     Usage (Y) */
    0x15U, 0x81U,                /*     Logical Minimum (-127) */
    0x25U, 0x7FU,                /*     Logical Maximum (127) */
    0x75U, 0x08U,                /*     Report Size (8) */
    0x95U, 0x02U,                /*     Report Count (2) */
    0x81U, 0x06U,                /*     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position) */
    0xC0U,                       /*   End Collection */
    0xC0U,                       /* End Collection */
};

/* HID Device Service Name : "Mouse n Keyboard" */
static char * hidd_demo_service_name = "Mouse n Keyboard";

/* HID Device Service Description : "Remote Presentation Controller" */
static char *hidd_demo_service_desc = "Remote Presentation Controller";

/* HID Device Provider Name : "beken" */
static char *hidd_demo_provider_name = "beken";

static uint8_t hidd_demo_sub_class = BK_HID_CLASS_COM;

static void hidd_dump_bytes (uint8_t *buffer, uint16_t length)
{
    char hex_stream[49U];
    char char_stream[17U];
    uint32_t i;
    uint16_t offset, count;
    uint8_t c;
    int32_t ret = 0;

    BTHIDD_LOGI("\n");
    BTHIDD_LOGI("-- Dumping %d Bytes --\n",
    (int)length);

    BTHIDD_LOGI(
    "-------------------------------------------------------------------\n");

    count = 0U;
    offset = 0U;
    for(i = 0U; i < length; i ++)
    {
        c =  buffer[i];

        if(offset + 3 >= sizeof(hex_stream))
        {
            BTHIDD_LOGE("!!!!!!!!! overflow !!!!!\n");
            while(1);
            return;
        }

        ret = sprintf(&hex_stream[offset], "%02X ", c);
        if(ret != 3)
        {
            BTHIDD_LOGE("!!!!!!!!! overflow sprintf ret wrong %d !!!!!\n", ret);
            while(1);
            return;
        }

        if(count >= sizeof(char_stream))
        {
            BTHIDD_LOGE("!!!!!!!!! overflow2 !!!!!\n");
            while(1);
            return;
        }

        if ( (c >= 0x20U) && (c <= 0x7EU) )
        {
            char_stream[count] = c;
        }
        else
        {
            char_stream[count] = '.';
        }

        count ++;
        offset += 3U;

        if(16U == count)
        {
            char_stream[count] = '\0';
            count = 0U;
            offset = 0U;

            BTHIDD_LOGI("%s   %s\n",
            hex_stream, char_stream);

            os_memset(hex_stream, 0, sizeof(hex_stream));
            os_memset(char_stream, 0, sizeof(char_stream));
        }
    }

    if(offset != 0U)
    {
        if(count >= sizeof(char_stream))
        {
            BTHIDD_LOGE("!!!!!!!!! overflow2 !!!!!\n");
            while(1);
            return;
        }

        char_stream[count] = '\0';

        /* Maintain the alignment */
        BTHIDD_LOGI("%-48s   %s\n",
        hex_stream, char_stream);
    }

    BTHIDD_LOGI(
    "-------------------------------------------------------------------\n");

    BTHIDD_LOGI("\n");
    return;
}



static void bt_hidd_callback(bk_hidd_cb_event_t event, bk_hidd_cb_param_t *param)
{
    switch (event)
    {
        case BK_HIDD_INIT_EVT:
            if(param->init.status == BK_HIDD_SUCCESS)
            {
                BTHIDD_LOGI("%s, init Success \r\n", __func__);
            }else
            {
                BTHIDD_LOGE("%s, init Fail \r\n", __func__);
            }
        break;
        case BK_HIDD_OPEN_EVT:
            if(param->open.status == BK_HIDD_SUCCESS)
            {
                if(param->open.conn_status == BK_HIDD_CONN_STATE_CONNECTED)
                {
                    BTHIDD_LOGI("%s, conn ind: 0x%02x:0x%02x:0x%02x:0x%02x:0x%02:0x%02x\r\n", __func__,
                                param->open.bd_addr[0],
                                param->open.bd_addr[1],
                                param->open.bd_addr[2],
                                param->open.bd_addr[3],
                                param->open.bd_addr[4],
                                param->open.bd_addr[5]);
                    os_memcpy(hd_param.addr, param->open.bd_addr, BK_BD_ADDR_LEN);
                    if (BK_BTPAN_STATE_DISCONNECTED == s_pan_state)
                    {
                        bk_bt_pan_connect(param->open.bd_addr, BK_PAN_ROLE_PANU, BK_PAN_ROLE_NAP);
                    }
                }
            }
        break;
        case BK_HIDD_SET_PROTOCOL_EVT:
        {
            bk_hidd_protocol_mode_t mode = param->set_protocol.protocol_mode;
            uint8_t res_status = BK_HID_PAR_HANDSHAKE_RSP_SUCCESS;
            if(mode == BK_HIDD_REPORT_MODE || mode == BK_HIDD_BOOT_MODE)
            {
                hd_param.protocol_mode = mode;
                BTHIDD_LOGI("%s set protocol mode: %d\r\n", __func__, (uint8_t)mode);
            }else
            {
                res_status = BK_HID_PAR_HANDSHAKE_RSP_ERR_INVALID_PARAM;
            }
            bk_bt_hid_device_send_response(BK_HIDD_MESSAGE_TYPE_SET_PROTOCOL, res_status, 0, NULL);
        }
        break;
        case BK_HIDD_GET_PROTOCOL_EVT:
        {
            BTHIDD_LOGI("%s get protocol mode: %d\r\n", __func__, (uint8_t)hd_param.protocol_mode);
            if(hd_param.protocol_mode == BK_HIDD_REPORT_MODE || hd_param.protocol_mode == BK_HIDD_BOOT_MODE)
            {
                uint8_t data[1] = {0};
                data[0] |= (1<<hd_param.protocol_mode); 
                bk_bt_hid_device_send_response(BK_HIDD_MESSAGE_TYPE_GET_PROTOCOL, BK_HID_PAR_HANDSHAKE_RSP_SUCCESS, 1, data);
            }else
            {
                bk_bt_hid_device_send_response(BK_HIDD_MESSAGE_TYPE_GET_PROTOCOL, BK_HID_PAR_HANDSHAKE_RSP_ERR_UNSUPPORTED_REQ, 0, NULL);
            }
        }
        break;
        case BK_HIDD_SEND_REPORT_EVT:
        {
            BTHIDD_LOGI("report send success \r\n");
            if(report_flag)
            {
                keyboard_report[1] = 0;
                keyboard_report[3] = 0;
                bk_bt_hid_device_send_report(9, keyboard_report);
                report_flag = 0;
            }
        }
        break;
        case BK_HIDD_SET_REPORT_EVT:
        {
            BTHIDD_LOGI("set report Event, report id:%d, len:%d \r\n", param->set_report.report_id, param->set_report.len);
            bk_hidd_handshake_error_t response = BK_HID_PAR_HANDSHAKE_RSP_ERR_INVALID_REP_ID;
            switch (param->set_report.report_type)
            {
                case BK_HIDD_REPORT_TYPE_INPUT:
                    BTHIDD_LOGI("input evt !\r\n");
                    if(param->set_report.report_id == KEY_BOARD_REPORT_ID)
                    {
                        if(param->set_report.len == 9)
                        {
                            os_memcpy(hid_demo_input_report_1, param->set_report.data, 9);
                            response = BK_HID_PAR_HANDSHAKE_RSP_SUCCESS;
                        }else
                        {
                            response = BK_HID_PAR_HANDSHAKE_RSP_ERR_INVALID_PARAM;
                        }
                    }else if(param->set_report.report_id == MOUSE_REPORT_ID)
                    {
                        if(param->set_report.len == 4)
                        {
                            os_memcpy(hid_demo_input_report_2, param->set_report.data, 4);
                            response = BK_HID_PAR_HANDSHAKE_RSP_SUCCESS;
                        }else
                        {
                            response = BK_HID_PAR_HANDSHAKE_RSP_ERR_INVALID_PARAM;
                        }
                    }else
                    {
                        BTHIDD_LOGI("input evt error report id!!\r\n");
                    }
                break;
                case BK_HIDD_REPORT_TYPE_OUTPUT:
                    BTHIDD_LOGI("output evt !\r\n");
                    if(param->set_report.report_id == KEY_BOARD_REPORT_ID)
                    {
                        if(param->set_report.len == 2)
                        {
                            os_memcpy(hid_demo_output_report_1, param->set_report.data, 2);
                        }else
                        {
                            response = BK_HID_PAR_HANDSHAKE_RSP_ERR_INVALID_PARAM;
                        }
                    }
                break;
                case BK_HIDD_REPORT_TYPE_FEATURE:
                    BTHIDD_LOGI("feature evt !\r\n");
                    response = BK_HID_PAR_HANDSHAKE_RSP_ERR_INVALID_PARAM;
                break;
                default:
                    BTHIDD_LOGE("Invalid Set Request: 0x%02X\n", param->set_report.report_type);
                    break;
            }
            bk_bt_hid_device_send_response(BK_HIDD_MESSAGE_TYPE_SET_REPORT, (uint8_t)response, 0, NULL);
        }
        break;
        case BK_HIDD_GET_REPORT_EVT:
        {
            BTHIDD_LOGI(" HID Device Get Report, type:%d, report_id:%d \r\n", param->get_report.report_type, param->get_report.report_id);
            uint8_t len = 0;
            bk_hidd_handshake_error_t sta = BK_HID_PAR_HANDSHAKE_RSP_ERR_INVALID_REP_ID;
            uint8_t *data = NULL;
            switch (param->get_report.report_type)
            {
                case BK_HIDD_REPORT_TYPE_INPUT:
                if(param->get_report.report_id == KEY_BOARD_REPORT_ID)
                {
                    data = hid_demo_input_report_1;
                    len = sizeof(hid_demo_input_report_1);
                    sta = BK_HID_PAR_HANDSHAKE_RSP_SUCCESS;
                }else if(param->get_report.report_id == MOUSE_REPORT_ID)
                {
                    data = hid_demo_input_report_2;
                    len = sizeof(hid_demo_input_report_2);
                    sta = BK_HID_PAR_HANDSHAKE_RSP_SUCCESS;
                }else
                {
                }
                break;
                case BK_HIDD_REPORT_TYPE_OUTPUT:
                if(param->get_report.report_id == KEY_BOARD_REPORT_ID)
                {
                    data = hid_demo_output_report_1;
                    len = sizeof(hid_demo_output_report_1);
                    sta = BK_HID_PAR_HANDSHAKE_RSP_SUCCESS;
                }
                break;
                case BK_HIDD_REPORT_TYPE_FEATURE:
                if(param->get_report.report_id == KEY_BOARD_REPORT_ID)
                {
                    data = hid_demo_feature_report_1;
                    len = sizeof(hid_demo_feature_report_1);
                    sta = BK_HID_PAR_HANDSHAKE_RSP_SUCCESS;
                }
                break;
                default:
                break;
            }
            if(len > param->get_report.buffer_size)
            {
                len = param->get_report.buffer_size;
            }
            bk_bt_hid_device_send_response(BK_HIDD_GET_REPORT_EVT, sta, len, data);
        }
        break;
        case BK_HIDD_INTR_DATA_EVT:
        BTHIDD_LOGI("HID receive int data, report_id:%d, report_type:%d \r\n", param->intr_data.report_id, param->intr_data.report_type);
        hidd_dump_bytes(param->intr_data.data, param->intr_data.len);
        break;
        case BK_HIDD_CLOSE_EVT:
        BTHIDD_LOGI("HID Device close, staus:%d, conn_status:%d\r\n", param->close.status, param->close.conn_status);
        break;
        case BK_HIDD_VC_UNPLUG_EVT:
        BTHIDD_LOGI("HID Device Unplug \r\n");
        break;
        case BK_HIDD_CONTROL_EVT:
        BTHIDD_LOGI("HID Device Control event, operation:%d \r\n", param->ctrl_op.op);
        break;
        case BK_HIDD_DEINIT_EVT:
        BTHIDD_LOGI("HID Device deinit \r\n");
        break;
        default :
        break;
       
    }
    
}

void bt_hidd_init(void)
{
    bk_bt_hid_device_register_callback(bt_hidd_callback);
    bk_bt_hid_device_init();
    bk_hidd_app_param_t param = {0};
    param.name = hidd_demo_service_name;
    param.description = hidd_demo_service_desc;
    param.provider = hidd_demo_provider_name;
    param.subclass = &hidd_demo_sub_class;
    param.desc_list = (uint8_t *)hidd_demo_report_desc;
    param.desc_list_len = sizeof(hidd_demo_report_desc);
    bk_bt_hid_device_register_app(&param);
    BTHIDD_LOGI("%s \r\n", __func__);
}

void bt_hidd_deinit(void)
{
    bk_bt_hid_device_deinit();
}

