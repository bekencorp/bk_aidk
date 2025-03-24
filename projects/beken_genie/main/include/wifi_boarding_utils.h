#pragma once

typedef enum
{
    BOARDING_OP_UNKNOWN = 0,
    BOARDING_OP_STATION_START = 1,
    BOARDING_OP_SOFT_AP_START = 2,
    BOARDING_OP_SERVICE_UDP_START = 3,
    BOARDING_OP_SERVICE_TCP_START = 4,
    BOARDING_OP_SET_CS2_DID = 5,
    BOARDING_OP_SET_CS2_APILICENSE = 6,
    BOARDING_OP_SET_CS2_KEY = 7,
    BOARDING_OP_SET_CS2_INIT_STRING = 8,
    BOARDING_OP_SRRVICE_CS2_START = 9,
    BOARDING_OP_BLE_DISABLE = 10,
    BOARDING_OP_SET_WIFI_CHANNEL = 11,
    BOARDING_OP_AGORA_AGENT_RSP = 12,
    BOARDING_OP_SET_AGORA_AGENT_INFO = 13,
    BOARDING_OP_NET_PAN_START = 14,
} boarding_opcode_t;

typedef void (*ble_boarding_op_cb_t)(uint16_t opcode, uint16_t length, uint8_t *data);
typedef struct
{
    char *ssid_value;
    char *password_value;
    ble_boarding_op_cb_t cb;
    uint8_t boarding_notify[2];
    uint16_t ssid_length;
    uint16_t password_length;
} ble_boarding_info_t;

int wifi_boarding_init(ble_boarding_info_t *info);
int wifi_boarding_deinit();
int wifi_boarding_adv_start(void);
int wifi_boarding_adv_stop(void);
int wifi_boarding_notify(uint8_t *data, uint16_t length);

