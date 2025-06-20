#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>

#include "cli.h"
#include "bk_genie_comm.h"
#include "boarding_service.h"
#if CONFIG_NET_PAN
#include "pan_service.h"
#endif

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

static bk_genie_boarding_info_t *bk_genie_boarding_info = NULL;
//static p2p_cs2_key_t *p2p_cs2_key = NULL;

bk_genie_boarding_info_t * bk_genie_get_boarding_info(void)
{
    return bk_genie_boarding_info;
}

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
    bk_genie_msg_t msg;
    LOGW("%s, opcode: %04X, length: %u\n", __func__, opcode, length);

    msg.event = opcode;
    switch (opcode)
    {
        case BOARDING_OP_STATION_START:
        case BOARDING_OP_SOFT_AP_START:
        case BOARDING_OP_SERVICE_UDP_START:
        case BOARDING_OP_SERVICE_TCP_START:
        case BOARDING_OP_BLE_DISABLE:
        case BOARDING_OP_SET_WIFI_CHANNEL:
        case BOARDING_OP_NET_PAN_START:
#if CONFIG_BK_MODEM
        case BOARDING_OP_START_BK_MODEM:
#endif
        case BOARDING_OP_SYNC_SUPPORTED_ENGINE:
        case BOARDING_OP_SYNC_SUPPORTED_NETWORK:
        {
            msg.param = 0;
            bk_genie_send_msg(&msg);
        }
        break;

        case BOARDING_OP_START_WIFI_SCAN:
#if CONFIG_STA_AUTO_RECONNECT
        case BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME:
#endif
        case BOARDING_OP_AGENT_RSP:
        {
            if (length > 0) {
                char *payload = os_zalloc(length + 1);

                os_memcpy(payload, data, length);
                msg.param = (uint32_t)payload;
            } else
                msg.param = 0;
            bk_genie_send_msg(&msg);
        }
        break;

        default:
        {
            LOGI("UNSUPPORT OP CODE\n");
            unsigned char payload = 'a';
            //100 is beken private definition, means unsupport status code
            bk_genie_boarding_event_notify_with_data(opcode, 100, (char *)(&payload), 1);
        }
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
    else
    {
        LOGI("%s already initialised\n", __func__);
        return BK_OK;
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
