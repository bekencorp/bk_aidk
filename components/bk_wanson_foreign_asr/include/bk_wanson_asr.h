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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wanson_asr *wanson_asr_handle_t;

typedef int (*wanson_asr_result_notify)(wanson_asr_handle_t wanson_asr, char *result, void *params);


typedef struct
{
    uint32_t pool_size;                                 /*!< the size (unit byte) of ringbuffer pool saved speaker data need to play */
    wanson_asr_result_notify asr_result_notify;         /*!< call this notify when wanson asr get result */
    void *usr_data;                                     /*!< the parameter of asr_result_notify callback */
} wanson_asr_cfg_t;

#define DEFAULT_WANSON_ASR_CONFIG() {       \
    .pool_size = 3840,                      \
    .asr_result_notify = NULL,              \
    .usr_data = NULL,                       \
}

wanson_asr_handle_t bk_wanson_asr_create(wanson_asr_cfg_t *config);

bk_err_t bk_wanson_asr_destroy(wanson_asr_handle_t wanson_asr);

bk_err_t bk_wanson_asr_start(wanson_asr_handle_t wanson_asr);

bk_err_t bk_wanson_asr_stop(wanson_asr_handle_t wanson_asr);

int bk_wanson_asr_data_write(wanson_asr_handle_t wanson_asr, int16_t *buffer, uint32_t len);
void bk_wanson_asr_set_spk_play_flag(uint8 spk_play_flag);

#ifdef __cplusplus
}
#endif

