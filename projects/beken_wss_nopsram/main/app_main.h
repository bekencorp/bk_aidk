#pragma once

#include <stdint.h>


typedef enum
{
    WSS_EVT_SERVER_HELLO           = 0,
    WSS_EVT_SERVER_SESSION_UPDATED,
    WSS_EVT_SERVER_BUF_COMMITED,
    WSS_EVT_SERVER_RSP_CREATED,
    WSS_EVT_SERVER_RSP_AUDIO_DONE,

    WSS_EVT_HELLO,
    WSS_EVT_SESSION_UPDATE,
    WSS_EVT_AUDIO_BUF_APPEND,
    WSS_EVT_AUDIO_BUF_CLEAR,
    WSS_EVT_AUDIO_BUF_COMMIT,
    WSS_EVT_RSP_CREATE,

} wss_evt_type_t;

void volume_set_abs(uint8_t level, uint8_t has_precision);
uint32_t volume_get_current();
uint32_t volume_get_level_count();
void wss_event_init(void);
bk_err_t websocket_event_send_msg(uint32_t event, uint32_t param);

