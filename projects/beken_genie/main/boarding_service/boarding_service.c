#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>

#include "cli.h"
#include "bk_genie_comm.h"
#include "boarding_service.h"
#include "pan_service.h"

//#include "bk_genie_cs2_service.h"
//#include "wifi_boarding_utils.h"

#include "components/bluetooth/bk_ble.h"
#include "components/bluetooth/bk_dm_ble.h"
#include "components/bluetooth/bk_dm_bluetooth.h"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define TAG "db-bd"

#define ADV_MAX_SIZE (251)
#define ADV_NAME_HEAD "bk_genie"

#define ADV_TYPE_FLAGS                      (0x01)
#define ADV_TYPE_LOCAL_NAME                 (0x09)
#define ADV_TYPE_SERVICE_UUIDS_16BIT        (0x14)
#define ADV_TYPE_SERVICE_DATA               (0x16)
#define ADV_TYPE_MANUFACTURER_SPECIFIC      (0xFF)

#define BEKEN_COMPANY_ID                    (0x05F0)

#define BOARDING_UUID                       (0xFE01)

static bk_genie_boarding_info_t *bk_genie_boarding_info = NULL;
//static p2p_cs2_key_t *p2p_cs2_key = NULL;


void bk_genie_boarding_event_notify(uint16_t opcode, int status)
{
    uint8_t data[] =
    {
        opcode & 0xFF, opcode >> 8,     /* opcode           */
                              status & 0xFF,                                                          /* status           */
                              0, 0,                                                                   /* payload length   */
    };

    LOGI("%s: %d, %d\n", __func__, opcode, status);
    wifi_boarding_notify(data, sizeof(data));
}

void bk_genie_boarding_event_notify_with_data(uint16_t opcode, int status, char *payload, uint16_t length)
{
    uint8_t data[1024] =
    {
        opcode & 0xFF, opcode >> 8,     /* opcode           */
                              status & 0xFF,                  /* status           */
                              length & 0xFF, length >> 8,     /* payload length   */
                              0,
    };

    if (length > 1024 - 5)
    {
        LOGE("size %d over flow\n", length);
        return;
    }

    os_memcpy(&data[5], payload, length);

    LOGI("%s: %d, %d\n", __func__, opcode, status);
    wifi_boarding_notify(data, length + 5);
}

void bk_genie_boarding_event_message(uint16_t opcode, int status)
{
    bk_genie_msg_t msg;

    msg.event = DBEVT_START_BOARDING_EVENT;
    msg.param = status << 16 | opcode;
    bk_genie_send_msg(&msg);
}

static void bk_genie_boarding_operation_handle(uint16_t opcode, uint16_t length, uint8_t *data)
{
    LOGW("%s, opcode: %04X, length: %u\n", __func__, opcode, length);

    switch (opcode)
    {
        case BOARDING_OP_STATION_START:
        {
            bk_genie_msg_t msg;

            msg.event = DBEVT_WIFI_STATION_CONNECT;
            msg.param = (uint32_t)bk_genie_boarding_info;
            bk_genie_send_msg(&msg);
        }
        break;

        case BOARDING_OP_START_WIFI_SCAN:
        {
            bk_genie_msg_t msg;

            msg.event = DBEVT_START_WIFI_SCAN;
            msg.param = (uint32_t)bk_genie_boarding_info;
            bk_genie_send_msg(&msg);
        }
        break;

        case BOARDING_OP_SOFT_AP_START:
        {
            bk_genie_msg_t msg;

            msg.event = DBEVT_WIFI_SOFT_AP_TURNING_ON;
            msg.param = (uint32_t)bk_genie_boarding_info;
            bk_genie_send_msg(&msg);
        }
        break;

        case BOARDING_OP_AGORA_AGENT_RSP:
        {
            bk_genie_msg_t msg;
            char *payload = os_zalloc(length + 1);

            os_memcpy(payload, data, length);
            msg.event = DBEVT_START_AGORA_AGENT_RSP;
            msg.param = (uint32_t)payload;
            bk_genie_send_msg(&msg);
        }
        break;

#if CONFIG_STA_AUTO_RECONNECT
        case BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME:
        {
extern uint8_t first_time_for_network_provisioning;
            LOGI("BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME\r\n");
            if (*data)
                first_time_for_network_provisioning = false;
        }
        break;
#endif

        case BOARDING_OP_SERVICE_UDP_START:
        {
            bk_genie_msg_t msg;

            msg.event = DBEVT_LAN_UDP_SERVICE_START_REQUEST;
            msg.param = 0;
            bk_genie_send_msg(&msg);
        }
        break;

        case BOARDING_OP_SERVICE_TCP_START:
        {
            bk_genie_msg_t msg;

            msg.event = DBEVT_LAN_TCP_SERVICE_START_REQUEST;
            msg.param = 0;
            bk_genie_send_msg(&msg);
        }
        break;

#if 0

        case BOARDING_OP_SET_CS2_DID:
        {
            if (p2p_cs2_key == NULL)
            {
                p2p_cs2_key = os_malloc(sizeof(p2p_cs2_key_t));

                if (p2p_cs2_key == NULL)
                {
                    LOGE("malloc p2p_cs2_key\n");
                    break;
                }

                os_memset(p2p_cs2_key, 0, sizeof(p2p_cs2_key_t));
            }

            if (strlen(p2p_cs2_key->did))
            {
                LOGE("Already has did %s\n", p2p_cs2_key->did);
                break;
            }

            if (length > sizeof(p2p_cs2_key->did))
            {
                LOGE("payload[%d] > did size[%d]\n", length, sizeof(p2p_cs2_key->did));
                break;
            }

            os_memcpy(p2p_cs2_key->did, data, length);

            LOGI("did: %s\n", p2p_cs2_key->did);

            bk_genie_boarding_event_message(opcode, BK_OK);
        }
        break;

        case BOARDING_OP_SET_CS2_APILICENSE:
        {
            if (p2p_cs2_key == NULL)
            {
                p2p_cs2_key = os_malloc(sizeof(p2p_cs2_key_t));

                if (p2p_cs2_key == NULL)
                {
                    LOGE("malloc p2p_cs2_key\n");
                    break;
                }

                os_memset(p2p_cs2_key, 0, sizeof(p2p_cs2_key_t));
            }

            if (strlen(p2p_cs2_key->apilicense))
            {
                LOGE("Already has apilicense %s\n", p2p_cs2_key->apilicense);
                break;
            }

            if (length > sizeof(p2p_cs2_key->apilicense))
            {
                LOGE("payload[%d] > apilicense size[%d]\n", length, sizeof(p2p_cs2_key->apilicense));
                break;
            }

            os_memcpy(p2p_cs2_key->apilicense, data, length);

            LOGI("apilicense: %s\n", p2p_cs2_key->apilicense);

            bk_genie_boarding_event_message(opcode, BK_OK);
        }
        break;

        case BOARDING_OP_SET_CS2_KEY:
        {
            if (p2p_cs2_key == NULL)
            {
                p2p_cs2_key = os_malloc(sizeof(p2p_cs2_key_t));

                if (p2p_cs2_key == NULL)
                {
                    LOGE("malloc p2p_cs2_key\n");
                    break;
                }

                os_memset(p2p_cs2_key, 0, sizeof(p2p_cs2_key_t));
            }

            if (strlen(p2p_cs2_key->key))
            {
                LOGE("Already key %s\n", p2p_cs2_key->key);
                break;
            }

            if (length > sizeof(p2p_cs2_key->key))
            {
                LOGE("payload[%d] > key size[%d]\n", length, sizeof(p2p_cs2_key->key));
                break;
            }

            os_memcpy(p2p_cs2_key->key, data, length);

            LOGI("key: %s\n", p2p_cs2_key->key);

            bk_genie_boarding_event_message(opcode, BK_OK);
        }
        break;

        case BOARDING_OP_SET_CS2_INIT_STRING:
        {
            if (p2p_cs2_key == NULL)
            {
                p2p_cs2_key = os_malloc(sizeof(p2p_cs2_key_t));

                if (p2p_cs2_key == NULL)
                {
                    LOGE("malloc p2p_cs2_key\n");
                    break;
                }

                os_memset(p2p_cs2_key, 0, sizeof(p2p_cs2_key_t));
            }

            if (strlen(p2p_cs2_key->initstring))
            {
                LOGE("Already has initstring %s\n", p2p_cs2_key->initstring);
                break;
            }

            if (length > sizeof(p2p_cs2_key->initstring))
            {
                LOGE("payload[%d] > initstring size[%d]\n", length, sizeof(p2p_cs2_key->initstring));
                break;
            }


            os_memcpy(p2p_cs2_key->initstring, data, length);

            LOGI("initstring: %s\n", p2p_cs2_key->initstring);

            bk_genie_boarding_event_message(opcode, BK_OK);
        }
        break;

        case BOARDING_OP_SRRVICE_CS2_START:
        {
            if (p2p_cs2_key == NULL)
            {
                LOGE("malloc p2p_cs2_key\n");
                break;
            }

            if (p2p_cs2_key->cs2_started)
            {
                LOGE("CS2 already started  %x\n", p2p_cs2_key->cs2_started);
                break;
            }

            strcat(p2p_cs2_key->apilicense, ":");
            strcat(p2p_cs2_key->apilicense, p2p_cs2_key->key);
            strcat(p2p_cs2_key->initstring, ":");
            strcat(p2p_cs2_key->initstring, p2p_cs2_key->key);
            p2p_cs2_key->cs2_started = true;

            bk_genie_msg_t msg;

            msg.event = DBEVT_P2P_CS2_SERVICE_START_REQUEST;
            msg.param = (uint32_t)p2p_cs2_key;
            bk_genie_send_msg(&msg);
        }
        break;
#endif

        case BOARDING_OP_BLE_DISABLE:
        {
            bk_genie_msg_t msg;

            msg.event = DBEVT_BLE_DISABLE;
            msg.param = 0;
            bk_genie_send_msg(&msg);
        }
        break;

        case BOARDING_OP_SET_WIFI_CHANNEL:
        {
            STREAM_TO_UINT16(bk_genie_boarding_info->channel, data);

            LOGI("%s, BOARDING_OP_SET_WIFI_CHANNEL: %u\n", __func__, bk_genie_boarding_info->channel);

        }
        break;

        case BOARDING_OP_NET_PAN_START:
        {
            bk_genie_msg_t msg;

            msg.event = DBEVT_NET_PAN_REQUEST;
            bk_genie_send_msg(&msg);
        }
#if CONFIG_BK_MODEM
        case BOARDING_OP_START_BK_MODEM:
        {
            bk_genie_msg_t msg;

            msg.event = DBEVT_START_BK_MODEM;
            bk_genie_send_msg(&msg);
        }
#endif
        break;
    }
}

int bk_genie_boarding_init(void)
{
    LOGI("%s\n", __func__);

    if (bk_genie_boarding_info == NULL)
    {
        bk_genie_boarding_info = os_malloc(sizeof(bk_genie_boarding_info_t));

        if (bk_genie_boarding_info == NULL)
        {
            LOGE("bk_genie_boarding_info malloc failed\n");

            goto error;
        }

        os_memset(bk_genie_boarding_info, 0, sizeof(bk_genie_boarding_info_t));
    }

    bk_genie_boarding_info->boarding_info.cb = bk_genie_boarding_operation_handle;

    wifi_boarding_init(&bk_genie_boarding_info->boarding_info);

#if CONFIG_NET_PAN
    pan_service_init();
#endif
    return BK_OK;
error:
    return BK_FAIL;
}

int bk_genie_boarding_deinit(void)
{
    LOGI("%s\n", __func__);

    wifi_boarding_adv_stop();
    wifi_boarding_deinit();

    if(bk_genie_boarding_info)
    {
        os_free(bk_genie_boarding_info);
        bk_genie_boarding_info = NULL;
    }

#if CONFIG_NET_PAN
    pan_service_deinit();
#endif
    return 0;
}
