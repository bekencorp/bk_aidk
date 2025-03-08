#pragma once

typedef enum
{
    APP_EVT_ASR_WAKEUP = 0,
    APP_EVT_ASR_STANDBY,
} app_evt_type_t;


void app_event_init(void);

