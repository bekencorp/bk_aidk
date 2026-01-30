// Copyright 2025-2026 Beken
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

typedef struct unisound_asr *unisound_asr_handle_t;

typedef int (*unisound_asr_result_notify)(unisound_asr_handle_t unisound_asr, char *result, void *params);

typedef struct
{
    uint32_t pool_size;                                 /*!< the size (unit byte) of ringbuffer pool saved speaker data need to play */
    unisound_asr_result_notify asr_result_notify;       /*!< call this notify when unisound asr get result */
    void *usr_data;                                     /*!< the parameter of asr_result_notify callback */
} unisound_asr_cfg_t;

#define DEFAULT_UNISOUND_ASR_CONFIG() {     \
    .pool_size = 3840*2,                      \
    .asr_result_notify = NULL,              \
    .usr_data = NULL,                       \
}

unisound_asr_handle_t bk_unisound_asr_create(unisound_asr_cfg_t *config);

bk_err_t bk_unisound_asr_destroy(unisound_asr_handle_t unisound_asr);

bk_err_t bk_unisound_asr_start(unisound_asr_handle_t unisound_asr);

bk_err_t bk_unisound_asr_stop(unisound_asr_handle_t unisound_asr);

int bk_unisound_asr_data_write(unisound_asr_handle_t unisound_asr, int16_t *buffer, uint32_t len);

#ifdef __cplusplus
}
#endif