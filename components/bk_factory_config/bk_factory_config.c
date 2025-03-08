// Copyright 2023-2024 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string.h>
#include <os/os.h>

#if (CONFIG_EASY_FLASH)
#include "bk_ef.h"
#include "bk_factory_config.h"

#define TAG "factory"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

const struct factory_config_t s_platform_config[] = {
    {"sys_initialized", "1"},   // first config used to check whether factory config initialized.
    {"key1", "value1"},
    {"key2", "value2"},
    {"key3", "value3"}
};

static const struct factory_config_t *s_user_reg_config = NULL;
static uint32_t s_user_config_len = 0;

void bk_factory_reset(void)
{
    int ret = 0;
    LOGI("reset factory config\r\n");
    // reset platform config
    for (size_t i = 0; i < sizeof(s_platform_config)/sizeof(s_platform_config[0]); i++) {
        ret = bk_set_env_enhance(s_platform_config[i].key, (const void *)s_platform_config[i].value,
                                 strlen(s_platform_config[i].value));
        if (ret != EF_NO_ERR) {
            LOGE("write platform factory config fail, key: %s,ret = %d\r\n", s_platform_config[i].key, ret);
        }
    }

    // reset user config
    for (size_t i = 0; i < s_user_config_len; i++) {
        ret = bk_set_env_enhance(s_user_reg_config[i].key, (const void *)s_user_reg_config[i].value,
                                 strlen(s_user_reg_config[i].value));
        if (ret != EF_NO_ERR) {
            LOGE("write user factory config fail, key: %s,ret = %d\r\n", s_user_reg_config[i].key, ret);
        }
    }
}

void bk_factory_init(void)
{
    int ret = 0;
    char get_value[20] = {0};
    ret = bk_get_env_enhance(s_platform_config[0].key, (void *)&get_value, sizeof(get_value));
    if (ret <= 0) {
        LOGI("first initialize factory config\r\n");
        bk_factory_reset();
    } else {
        LOGI("factory config already initialized\r\n");
    }
}

static void test_factory(void)
{
    int ret = 0;
    char get_value[50] = {0};

    LOGI("read factory config\r\n");
    // read platform config
    for (size_t i = 0; i < sizeof(s_platform_config)/sizeof(s_platform_config[0]); i++) {
        ret = bk_get_env_enhance(s_platform_config[0].key, (void *)&get_value, sizeof(get_value));
        if (ret > 0) {
            get_value[ret] = 0;
            LOGI("key: %s, value = %s\r\n", s_platform_config[i].key, get_value);
        } else {
            LOGE("read platform factory config fail, key: %s, ret = %d\r\n", s_platform_config[i].key, ret);
        }
    }

    // read user config
    for (size_t i = 0; i < s_user_config_len; i++) {
        ret = bk_get_env_enhance(s_user_reg_config[0].key, (void *)&get_value, sizeof(get_value));
        if (ret > 0) {
            get_value[ret] = 0;
            LOGI("key: %s, value = %s\r\n", s_user_reg_config[i].key, get_value);
        } else {
            LOGE("read platform factory config fail, key: %s, ret = %d\r\n", s_user_reg_config[i].key, ret);
        }
    }
}

void bk_regist_factory_user_config(const struct factory_config_t *config, uint16_t config_len)
{
    BK_ASSERT(config != NULL);
    BK_ASSERT(config_len > 0);
    s_user_reg_config = config;
    s_user_config_len = config_len;
    // test_factory();
}

#endif