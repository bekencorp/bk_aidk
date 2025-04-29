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

#ifndef __BK_FACTORY_CONFIG_H
#define __BK_FACTORY_CONFIG_H

#include <os/os.h>

#ifdef __cplusplus
extern "C" {
#endif

struct factory_config_t {
    char *key;
    void *value;
    uint8_t defval_size;
    uint8_t need_sram_cache;
    uint16_t value_max_size;
};

void bk_factory_init(void);
void bk_factory_reset(void);

// Note: config pointer referred struct need define on flash.
void bk_regist_factory_user_config(const struct factory_config_t *config, uint16_t config_len);

void bk_config_sync_flash(void);

bk_err_t bk_config_sync_flash_safely(void);

// return 0: success, -1: fail.
int bk_config_write(const char *key, const void *value, int value_len);

// return number of value size.
int bk_config_read(const char *key, void *value, int value_len);

#ifdef __cplusplus
}
#endif

#endif