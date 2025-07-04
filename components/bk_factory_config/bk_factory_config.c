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
#include <stdlib.h>
#include <os/os.h>
#include <os/mem.h>
#if (CONFIG_EASY_FLASH)
#include "bk_ef.h"
#include "bk_cli.h"
#include "bk_factory_config.h"
#include "driver/flash.h"

#define TAG "factory"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static const uint32_t s_factory_volume = 7;

const struct factory_config_t s_platform_config[] = {
    {"sys_initialized", (void *)"1", 1, BK_FALSE, 1},   // first config used to check whether factory config initialized.
    {"volume", (void *)&s_factory_volume, 4, BK_TRUE, 4},
    {"d_network_id", (void *)"\0", 1, BK_TRUE, 198},
    {"d_agent_info", (void *)"\0", 1, BK_TRUE, 588},
};

static const struct factory_config_t *s_user_reg_config = NULL;
static uint32_t s_user_config_len = 0;
static uint16_t s_value_max_len = 0;

static void bk_reboot_sync_config(void);

static inline int bk_factory_read_flash(const char *key, void *value, int value_len)
{
    int ret = 0;

    // read platform config
    ret = bk_get_env_enhance(key, (void *)value, value_len);
    if (ret <= 0) {
        LOGE("read platform factory config fail, key: %s, ret = %d\r\n", key, ret);
    }
    return ret;
}

static inline int bk_factory_write_flash(const char *key, const void *value, int value_len)
{
    int ret = 0;
    ret = bk_set_env_enhance(key, value, value_len);
    if (ret != EF_NO_ERR) {
        LOGE("write platform factory config fail, key: %s,ret = %d\r\n", key, ret);
    }
    return ret;
}

struct factory_config_map_t {
    char *key;
    uint8_t *ptr;
    uint16_t size;
    uint16_t valid_len;
};

struct cache_config_info_t {
    uint16_t config_cache_len;
    uint16_t config_cache_num;
};

static uint8_t *s_factory_data = NULL;
static struct factory_config_map_t *s_factory_cache_map = NULL;
static uint32_t s_factory_cache_num = 0;

static inline void compute_max_len(uint16_t len)
{
    if (len > s_value_max_len) {
        s_value_max_len = len;
    }
}

static void get_config_cache_len(struct cache_config_info_t *info)
{
    for (size_t i = 0; i < sizeof(s_platform_config)/sizeof(s_platform_config[0]); i++) {
        if (s_platform_config[i].need_sram_cache) {
            info->config_cache_len += s_platform_config[i].value_max_size;
            info->config_cache_num++;
            compute_max_len(s_platform_config[i].value_max_size);
        }
    }
    for (size_t i = 0; i < s_user_config_len; i++) {
        if (s_user_reg_config[i].need_sram_cache) {
            info->config_cache_len += s_user_reg_config[i].value_max_size;
            info->config_cache_num++;
            compute_max_len(s_user_reg_config[i].value_max_size);
        }
    }
    s_factory_cache_num = info->config_cache_num;
}

static void set_cache_map_table(struct cache_config_info_t *info)
{
    BK_ASSERT(s_factory_cache_map != NULL);
    if (info->config_cache_num == 0) {
        LOGI("there is no config in cache\r\n");
        return;
    }
    uint32_t cnt = 0;
    uint32_t offset = 0;
    for (size_t i = 0; i < sizeof(s_platform_config)/sizeof(s_platform_config[0]); i++) {
        if (s_platform_config[i].need_sram_cache) {
            s_factory_cache_map[cnt].key = s_platform_config[i].key;
            s_factory_cache_map[cnt].ptr = (uint8_t *)&s_factory_data[offset];
            s_factory_cache_map[cnt].size = s_platform_config[i].value_max_size;
            s_factory_cache_map[cnt].valid_len = 0;
            cnt++;
            offset += s_platform_config[i].value_max_size;
        }
    }
    for (size_t i = 0; i < s_user_config_len; i++) {
        if (s_user_reg_config[i].need_sram_cache) {
            s_factory_cache_map[cnt].key = s_user_reg_config[i].key;
            s_factory_cache_map[cnt].ptr = (uint8_t *)&s_factory_data[offset];
            s_factory_cache_map[cnt].size = s_user_reg_config[i].value_max_size;
            s_factory_cache_map[cnt].valid_len = 0;
            cnt++;
            offset += s_user_reg_config[i].value_max_size;
        }
    }
}

static void restore_cache_from_flash(void)
{
    int ret;
    for (size_t i = 0; i < s_factory_cache_num; i++) {
        // s_factory_cache_map[i].key
        ret = bk_factory_read_flash(s_factory_cache_map[i].key, (void *)s_factory_cache_map[i].ptr,
                              s_factory_cache_map[i].size);
        s_factory_cache_map[i].valid_len = ret;
        if (ret <= 0) {
            LOGE("read platform factory config fail, key: %s, ret = %d\r\n", s_factory_cache_map[i].key, ret);
        }
    }
}

static void bk_factory_cache_init(void)
{
    struct cache_config_info_t cache_info = {0};
    get_config_cache_len(&cache_info);
    
    if (cache_info.config_cache_len > 0) {
        s_factory_data = (uint8_t *)os_malloc(cache_info.config_cache_len);
        if (s_factory_data == NULL) {
            LOGE("malloc s_factory_data fail\r\n");
            return;
        }
        uint32_t map_table_size = sizeof(struct factory_config_map_t) * cache_info.config_cache_num;
        s_factory_cache_map = (struct factory_config_map_t *)os_malloc(map_table_size);
        if (s_factory_cache_map == NULL) {
            os_free(s_factory_data);
            LOGE("malloc s_factory_cache_map fail\r\n");
        }
    }

    set_cache_map_table(&cache_info);

    restore_cache_from_flash();
}

void bk_factory_reset(void)
{
    LOGI("reset factory config\r\n");
    // reset platform config
    for (size_t i = 0; i < sizeof(s_platform_config)/sizeof(s_platform_config[0]); i++) {
        bk_factory_write_flash(s_platform_config[i].key, (const void *)s_platform_config[i].value,
                                s_platform_config[i].defval_size);
    }

    // reset user config
    for (size_t i = 0; i < s_user_config_len; i++) {
        bk_factory_write_flash(s_user_reg_config[i].key, (const void *)s_user_reg_config[i].value,
                                s_user_reg_config[i].defval_size);
    }

    bk_factory_cache_init();
}

static void test_factory(void)
{
    // int ret = 0;
    char get_value[50] = {0};

    LOGI("read factory config\r\n");
    // read platform config
    for (size_t i = 0; i < sizeof(s_platform_config)/sizeof(s_platform_config[0]); i++) {
        bk_factory_read_flash(s_platform_config[i].key, (void *)get_value, sizeof(get_value));
    }

    // read user config
    for (size_t i = 0; i < s_user_config_len; i++) {
        bk_factory_read_flash(s_user_reg_config[i].key, (void *)get_value, sizeof(get_value));
    }
}

static void cli_factory_read(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    uint32_t volume;
    int ret = bk_config_read("volume", (void *)&volume, sizeof(volume));
    LOGI("sram volume = %u, ret = %d\r\n", volume, ret);

    ret = bk_get_env_enhance("volume", (void *)&volume, sizeof(volume));
    LOGI("flash volume = %u, ret = %d\r\n", volume, ret);

}

static void cli_factory_write(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    static uint32_t s_volume = 20;
    int ret = bk_config_write("volume", (const void *)&s_volume, sizeof(s_volume));
    LOGI("volume = %u, ret = %d\r\n", s_volume, ret);
    s_volume++;
}

static void cli_factory_sync(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    bk_config_sync_flash();
}

static void cli_factory_reset(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    bk_factory_reset();
}

#define FACTORY_CMD_CNT (sizeof(s_factory_commands) / sizeof(struct cli_command))
static const struct cli_command s_factory_commands[] = {
    {"factory_read", "", cli_factory_read},
    {"factory_write", "", cli_factory_write},
    {"factory_sync", "", cli_factory_sync},
    {"factory_reset", "", cli_factory_reset},
};

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
        bk_factory_cache_init();
    }

    // test_factory();
    bk_reboot_callback_register(bk_reboot_sync_config);

    cli_register_commands(s_factory_commands, FACTORY_CMD_CNT);
}

void bk_regist_factory_user_config(const struct factory_config_t *config, uint16_t config_len)
{
    BK_ASSERT(config != NULL);
    BK_ASSERT(config_len > 0);
    s_user_reg_config = config;
    s_user_config_len = config_len;
}

static int find_cache_index(const char *key)
{
    for (size_t i = 0; i < s_factory_cache_num; i++) {
        if (strcmp(key, s_factory_cache_map[i].key) == 0) {
            return i;
        }
    }
    return -1;
}

int bk_config_read(const char *key, void *value, int value_len)
{
    BK_ASSERT(value_len > 0);
    if (s_factory_cache_map == NULL) {
        return 0;
    }
    int index = find_cache_index(key);
    if (index < 0) {
        return 0;
    }
    memcpy(value, s_factory_cache_map[index].ptr, value_len);
    return s_factory_cache_map[index].valid_len;
}

int bk_config_write(const char *key, const void *value, int value_len)
{
    BK_ASSERT(value_len > 0);
    if (s_factory_cache_map == NULL) {
        LOGW("config cache not init.\r\n");
        return -1;
    }
    int index = find_cache_index(key);
    if (index < 0) {
        LOGW("key not found inf cache.\r\n");
        return -1;
    }
    if (s_factory_cache_map[index].size >= value_len) {
        memcpy(s_factory_cache_map[index].ptr, value, value_len);
        s_factory_cache_map[index].valid_len = value_len;
    } else {
        return -1;
    }
    return 0;
}

static bool is_config_update(uint8_t *buffer, char *key, void *cache_value, uint16_t cache_len)
{
    int read_len = 0;
    read_len = bk_get_env_enhance(key, buffer, s_value_max_len);
#if BK_FACTORY_TEST
    bk_mem_dump_ex("cache value", (uint8_t *)cache_value, cache_len);
    if (read_len > 0) {
        bk_mem_dump_ex("flash value", (uint8_t *)buffer, read_len);
    }
#endif
    if (read_len <= 0 || read_len != cache_len) {
        return BK_TRUE;
    }
    return memcmp(buffer, cache_value, cache_len) != 0;
}

void bk_config_sync_flash(void)
{
    if (s_factory_cache_map == NULL) {
        return;
    }
    LOGI("save config to flash.\r\n");
    bool read_compare = BK_TRUE;  // if malloc fail, write to flash without comparison
    uint8_t *buffer = (uint8_t *)os_malloc(s_value_max_len);
    LOGD("s_value_max_len = %u\r\n", s_value_max_len);
    if (buffer == NULL) {
        read_compare = BK_FALSE;
    }
    for (size_t i = 0; i < s_factory_cache_num; i++) {
        LOGD("key = %s\r\n", s_factory_cache_map[i].key);
        if (read_compare == BK_FALSE || is_config_update(buffer, s_factory_cache_map[i].key,
            (void *)s_factory_cache_map[i].ptr, s_factory_cache_map[i].valid_len)) {
            LOGI("update %s\r\n", s_factory_cache_map[i].key);
            bk_factory_write_flash(s_factory_cache_map[i].key, (void *)s_factory_cache_map[i].ptr,
                                   s_factory_cache_map[i].valid_len);
        }
    }
    if (buffer != NULL) {
        os_free(buffer);
    }
}

static void bk_reboot_sync_config(void)
{
    if (rtos_is_in_interrupt_context()) {
        LOGE("in interrupt context, not sync config\r\n");
        return;
    }
    void bk_ef_set_check_lock(bool state);
    uint32_t int_mask = rtos_disable_int();
    bk_ef_set_check_lock(BK_FALSE);
    bk_config_sync_flash();
    bk_ef_set_check_lock(BK_TRUE);
    rtos_enable_int(int_mask);
}

#define BLE_ERASE_WAIT_TIMES_MAX 3000
bk_err_t bk_config_sync_flash_safely(void)
{
    uint32_t i = 0;
    uint32_t ble_erase_wait_times_max = BLE_ERASE_WAIT_TIMES_MAX;

    for (i = 0; i < ble_erase_wait_times_max; i++) {
        if(is_ble_erase_flash_ready() != BK_TRUE) {
            rtos_delay_milliseconds(2);
        } else {
            break;
        }
    }

    if(i >= ble_erase_wait_times_max)
    {
        LOGE("wait ble sleep period timeout, do not sync config to flash.\r\n");
        return BK_FAIL;
    }

    bk_config_sync_flash();
    return BK_OK;
}

#endif
