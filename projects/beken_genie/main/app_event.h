#pragma once

typedef enum
{
    APP_EVT_ASR_WAKEUP = 0,
    APP_EVT_ASR_STANDBY,
    APP_EVT_PAIRING_NETWORK,
    APP_EVT_RECONNECT_NETWORK,
    APP_EVT_CONNECT_NETWORK_FAIL,
    APP_EVT_RTC_CONNECTION_LOST,
    APP_EVT_AGENT_JOINED,
    APP_EVT_AGENT_OFFLINE,
    APP_EVT_LOW_VOLTAGE,
    APP_EVT_CHARGING,
} app_evt_type_t;


void app_event_init(void);
bk_err_t app_event_send_msg(uint32_t event, uint32_t param);

