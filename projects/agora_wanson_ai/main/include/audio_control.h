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
    AUDIO_MODE_IDLE = 0,
    AUDIO_MODE_ASR,
    AUDIO_MODE_BELL,
    AUDIO_MODE_RECORD,
    AUDIO_MODE_PLAY,
} audio_mode_t;

typedef enum
{
    AUDIO_CONTROL_IDLE = 0,
    AUDIO_CONTROL_ASR,
    AUDIO_CONTROL_BELL,
    AUDIO_CONTROL_RECORD,
    AUDIO_CONTROL_PLAY,
    AUDIO_CONTROL_EXIT,
} audio_control_op_t;

typedef struct
{
    audio_control_op_t op;
    void *param;
} audio_control_msg_t;

typedef struct
{
    RingBufferContext           aud_tx_rb;
    int8_t                     *tx_ring_buff;
    uint32_t                    tx_size;

    RingBufferContext           aud_rx_rb;
    int8_t                     *rx_ring_buff;
    uint32_t                    rx_size;
} audio_rb_ctx_t;


bk_err_t audio_control_send_msg(audio_control_op_t op, void *param);

bk_err_t audio_control_init(void);

bk_err_t audio_control_deinit(void);

bk_err_t audio_control_start(void);

bk_err_t audio_control_stop(void);

#ifdef __cplusplus
}
#endif

