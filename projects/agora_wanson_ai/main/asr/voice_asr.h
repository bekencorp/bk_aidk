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
#include "audio_record.h"

#include "wanson_asr.h"

#ifdef __cplusplus
extern "C" {
#endif

bk_err_t voice_asr_init(audio_record_t *record);
bk_err_t voice_asr_deinit(void);
bk_err_t voice_asr_start(void);
bk_err_t voice_asr_stop(void);

/* use wanson asr agorithm */
#define voice_asr_init wanson_asr_init
#define voice_asr_deinit wanson_asr_deinit
#define voice_asr_start wanson_asr_start
#define voice_asr_stop wanson_asr_stop

#ifdef __cplusplus
}
#endif

