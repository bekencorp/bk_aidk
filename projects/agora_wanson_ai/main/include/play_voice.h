// Copyright 2023-2024 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <os/os.h>
#include <driver/audio_ring_buff.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    PLAY_VOICE_IDLE = 0,
    PLAY_VOICE_PLAY,
    PLAY_VOICE_EXIT,
} play_voice_op_t;

typedef struct
{
    play_voice_op_t op;
    void *param;
} play_voice_msg_t;


bk_err_t play_voice_send_msg(play_voice_op_t op, void *param);

bk_err_t play_voice_init(RingBufferContext *spk_rb, audio_play_t *audio_play);

bk_err_t play_voice_deinit(void);

bk_err_t play_voice_start(void);

bk_err_t play_voice_stop(void);

#ifdef __cplusplus
}
#endif

