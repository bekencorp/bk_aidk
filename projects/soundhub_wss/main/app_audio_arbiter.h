#pragma once
#include <stdint.h>

typedef enum
{
    AUDIO_SOURCE_ENTRY_START,
    AUDIO_SOURCE_ENTRY_AI = AUDIO_SOURCE_ENTRY_START,
    AUDIO_SOURCE_ENTRY_A2DP,
    AUDIO_SOURCE_ENTRY_USER,
    AUDIO_SOURCE_ENTRY_END,
} AUDIO_SOURCE_ENTRY_EMUM;

typedef enum
{
    AUDIO_SOURCE_ENTRY_STATUS_IDLE,
    AUDIO_SOURCE_ENTRY_STATUS_STOP,
    AUDIO_SOURCE_ENTRY_STATUS_PLAY_PENDING,
    AUDIO_SOURCE_ENTRY_STATUS_PLAY,
} AUDIO_SOURCE_ENTRY_STATUS;

typedef enum
{
    AUDIO_SOURCE_ENTRY_CB_EVT_NEED_START,
    AUDIO_SOURCE_ENTRY_CB_EVT_IGNORE,
    AUDIO_SOURCE_ENTRY_CB_EVT_NEED_STOP,
} AUDIO_SOURCE_ENTRY_CB_EVT;

typedef enum
{
    AUDIO_SOURCE_ENTRY_ACTION_START_REQ,
    AUDIO_SOURCE_ENTRY_ACTION_STOP_REQ,
} AUDIO_SOURCE_ENTRY_ACTION;

typedef void (*audio_source_entry_cb)(AUDIO_SOURCE_ENTRY_CB_EVT evt, void *arg);


void app_audio_arbiter_reg_callback(AUDIO_SOURCE_ENTRY_EMUM entry, audio_source_entry_cb cb, void *arg);
AUDIO_SOURCE_ENTRY_STATUS app_audio_arbiter_report_source_req(AUDIO_SOURCE_ENTRY_EMUM entry, AUDIO_SOURCE_ENTRY_ACTION act);
AUDIO_SOURCE_ENTRY_EMUM app_audio_arbiter_get_current_play_entry(void);
void app_audio_arbiter_init(void);
void app_audio_arbiter_deinit(void);
