#include "cli.h"
#include "components/bluetooth/bk_dm_pan.h"
#include "pan_service.h"

static void pan_usage(void)
{
    CLI_LOGI("Usage:\n"
             "pan connect XX:XX:XX:XX:XX:XX\n"
             "pan pair_mode\n"
             "pan disconnect XX:XX:XX:XX:XX:XX\n"
            );

    return;
}

static void cmd_pan_demo(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    char *msg = NULL;
    int ret = 0;

    if (argc == 1)
    {
        goto __usage;
    }
    else if (os_strcmp(argv[1], "-h") == 0)
    {
        goto __usage;
    }
    else if (os_strcmp(argv[1], "connect") == 0)
    {
        if (argc >= 3)
        {
            uint8_t mac_final[6] = {0};
            uint32_t mac[6] = {0};

            //sscanf bug: cant detect uint8_t size point
            ret = sscanf(argv[2], "%02x:%02x:%02x:%02x:%02x:%02x", //argv[1], "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                         mac + 5,
                         mac + 4,
                         mac + 3,
                         mac + 2,
                         mac + 1,
                         mac);

            if (ret != 6)
            {
                CLI_LOGE("param err, need mac, %d %s\n", ret, argv[1]);
                return;
            }

            for (uint8_t i = 0; i < sizeof(mac_final) / sizeof(mac_final[0]); ++i)
            {
                mac_final[i] = mac[i];
            }


            CLI_LOGI("%s mac %02x:%02x:%02x:%02x:%02x:%02x\n", __func__,
                     mac_final[5],
                     mac_final[4],
                     mac_final[3],
                     mac_final[2],
                     mac_final[1],
                     mac_final[0]);


            ret = bk_bt_pan_connect(mac_final, BK_PAN_ROLE_PANU, BK_PAN_ROLE_NAP);

            if (ret)
            {
                goto __error;
            }
        }
        else
        {
            goto __usage;
        }
    }
    else if (os_strcmp(argv[1], "pair_mode") == 0)
    {
        void bk_bt_enter_pairing_mode(uint8_t is_visible);
        bk_bt_enter_pairing_mode(1);
    }
    else if (os_strcmp(argv[1], "write") == 0)
    {
        if (argc >= 3)
        {
            uint8_t mac_final[6] = {0};
            uint32_t mac[6] = {0};

            //sscanf bug: cant detect uint8_t size point
            ret = sscanf(argv[2], "%02x:%02x:%02x:%02x:%02x:%02x", //argv[1], "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                         mac + 5,
                         mac + 4,
                         mac + 3,
                         mac + 2,
                         mac + 1,
                         mac);

            if (ret != 6)
            {
                CLI_LOGE("param err, need mac, %d %s\n", ret, argv[1]);
                return;
            }

            for (uint8_t i = 0; i < sizeof(mac_final) / sizeof(mac_final[0]); ++i)
            {
                mac_final[i] = mac[i];
            }

            //72
            uint8_t test_data[] = {0x60, 0x00, 0x00, 0x00, 0x00, 0x20, 0x3A, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0x26, 0xB1, 0x40,
                                   0x87, 0x00, 0xFA, 0x95, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD2, 0x4E, 0x50, 0xFF,
                                   0xFE, 0x26, 0xB1, 0x40, 0x0E, 0x01, 0x02, 0x70, 0xD7, 0x19, 0x15, 0xE2
                                  };

            eth_data_t *p_eth_data = os_malloc(sizeof(eth_data_t) + sizeof(test_data));

            p_eth_data->dest[0] = 0x33;
            p_eth_data->dest[1] = 0x33;
            p_eth_data->dest[2] = 0xff;
            p_eth_data->dest[3] = 0x26;
            p_eth_data->dest[4] = 0xb1;
            p_eth_data->dest[5] = 0x40;

            bk_get_mac((uint8_t *)p_eth_data->src, MAC_TYPE_BLUETOOTH);

            p_eth_data->protocol = 0x86dd;
            p_eth_data->payload_len = sizeof(test_data);

            os_memcpy(p_eth_data->payload, test_data, p_eth_data->payload_len);

            bk_bt_pan_write(mac_final, p_eth_data);

            os_free(p_eth_data);
        }
    }
    else if (os_strcmp(argv[1], "disconnect") == 0)
    {
        if (argc >= 3)
        {
            uint8_t mac_final[6] = {0};
            uint32_t mac[6] = {0};

            //sscanf bug: cant detect uint8_t size point
            ret = sscanf(argv[2], "%02x:%02x:%02x:%02x:%02x:%02x", //argv[1], "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                         mac + 5,
                         mac + 4,
                         mac + 3,
                         mac + 2,
                         mac + 1,
                         mac);

            if (ret != 6)
            {
                CLI_LOGE("param err, need mac, %d %s\n", ret, argv[1]);
                return;
            }

            for (uint8_t i = 0; i < sizeof(mac_final) / sizeof(mac_final[0]); ++i)
            {
                mac_final[i] = mac[i];
            }


            CLI_LOGI("%s mac %02x:%02x:%02x:%02x:%02x:%02x\n", __func__,
                     mac_final[5],
                     mac_final[4],
                     mac_final[3],
                     mac_final[2],
                     mac_final[1],
                     mac_final[0]);


            ret = bk_bt_pan_disconnect(mac_final);

            if (ret)
            {
                goto __error;
            }
        }
        else
        {
            goto __usage;
        }
    }
    else if (os_strcmp(argv[1], "txmem") == 0)
    {
        pan_show_tx_data_cache_count();
    }
    else
    {
        goto __usage;
    }

    msg = CLI_CMD_RSP_SUCCEED;
    os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
    return;

__usage:
    pan_usage();

__error:
    msg = CLI_CMD_RSP_ERROR;
    os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
}

static const struct cli_command s_pan_commands[] =
{
    {"pan", "see -h", cmd_pan_demo},
};

int cli_pan_demo_init(void)
{
    return cli_register_commands(s_pan_commands, sizeof(s_pan_commands) / sizeof(s_pan_commands[0]));
}

int cli_pan_demo_deinit(void)
{
    return cli_unregister_commands(s_pan_commands, sizeof(s_pan_commands) / sizeof(s_pan_commands[0]));
}

