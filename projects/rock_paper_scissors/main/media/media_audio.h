#pragma once

#include <stdint.h>


enum
{
    TONE_ENUM_START,
    TONE_ENUM_CANT_DET = TONE_ENUM_START,
    TONE_ENUM_DETECTING,
    TONE_ENUM_DRAW,
    TONE_ENUM_GET_READY,
    TONE_ENUM_MY_SHOW,
    TONE_ENUM_PAPER,
    TONE_ENUM_PLS_STOP_HAND,
    TONE_ENUM_ROCK,
    TONE_ENUM_SCISSORS,
    TONE_ENUM_YOU_LOSS,
    TONE_ENUM_YOU_WIN,
    TONE_ENUM_YOUR_SHOW,
    TONE_ENUM_END,
};


int32_t audio_init(void);
int32_t audio_deinit(void);

int32_t audio_play(uint32_t tone_type, uint8_t sync);
int32_t audio_wait_play_end(void * (*cb)(void), void (*free_cb)(void *arg), uint32_t ms);
