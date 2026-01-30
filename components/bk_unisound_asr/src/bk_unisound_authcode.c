#include <os/os.h>
#include <os/mem.h>

#include <driver/flash.h>
#include <driver/flash_partition.h>
#include <vendor_flash_partition.h>

#include "bk_unisound_authcode.h"

/**
 * @brief Check if the unisound auth code exists in flash.
 *
 * This function checks if the unisound auth code is stored in the flash partition
 * BK_PARTITION_UNISOUND_CONFIG_USER.
 *
 * @return 1 if the auth code exists, 0 otherwise.
 */
bk_err_t check_unisound_auth_code(void)
{
    bk_logic_partition_t *unisound_partition = bk_flash_partition_get_info(BK_PARTITION_UNISOUND_CONFIG_USER);
    if (unisound_partition == NULL) {
        os_printf("BK_PARTITION_UNISOUND_CONFIG_USER not found, cannot check auth code\n");
        return BK_FAIL;
    }
    auth_code_head_t auth_head = {0};
    bk_err_t ret = bk_flash_read_bytes(unisound_partition->partition_start_addr, (uint8_t *)&auth_head, sizeof(auth_code_head_t));
    if (ret != BK_OK) {
        os_printf("bk_flash_read_bytes failed, ret: %d\n", ret);
        return BK_FAIL;
    }
    if (auth_head.type != UNISOUND_AUTH_CODE_TAB) {
        os_printf("auth code NULL\n");
        return BK_FAIL;
    }
    return BK_OK;
}

bk_err_t set_unisound_auth_code(uint8_t * authcode, uint32_t authcode_len)
{
    bk_err_t ret = BK_FAIL;
    if (!authcode) {
        os_printf("authcode is NULL.\n");
        return BK_FAIL;
    }
    if (authcode_len > AUTH_CODE_MAX_SIZE) {
        os_printf("authcode len is too long, max len is %d.\n", AUTH_CODE_MAX_SIZE);
        return BK_FAIL;
    }
    bk_logic_partition_t *unisound_partition = bk_flash_partition_get_info(BK_PARTITION_UNISOUND_CONFIG_USER);
    if (unisound_partition == NULL) {
        os_printf("BK_PARTITION_UNISOUND_CONFIG_USER not found, cannot set auth code\n");
        return BK_FAIL;
    }
    auth_code_head_t auth_head = {0};
    auth_head.type = UNISOUND_AUTH_CODE_TAB;
    auth_head.len = authcode_len;

    ret = bk_flash_write_bytes(unisound_partition->partition_start_addr, (uint8_t *)&auth_head, sizeof(auth_code_head_t));
    if (ret != BK_OK) {
        os_printf("bk_flash_write_bytes failed, ret: %d\n", ret);
        return ret;
    }
    ret = bk_flash_write_bytes(unisound_partition->partition_start_addr + sizeof(auth_code_head_t), authcode, auth_head.len);
    if (ret != BK_OK) {
        os_printf("bk_flash_write_bytes failed, ret: %d\n", ret);
        return ret;
    }
    return BK_OK;
}


/**
 * @brief Get the unisound auth code from flash.
 *
 * This function reads the unisound auth code from the flash partition BK_PARTITION_UNISOUND_CONFIG_USER.
 *
 * @return The auth code string if successful, NULL otherwise.
 */
uint8_t *get_unisound_auth_code(void)
{
    bk_err_t ret = BK_FAIL;
    bk_logic_partition_t *unisound_partition = bk_flash_partition_get_info(BK_PARTITION_UNISOUND_CONFIG_USER);
    if (unisound_partition == NULL) {
        os_printf("BK_PARTITION_UNISOUND_CONFIG_USER not found, cannot check auth code\n");
        return 0;
    }
    auth_code_head_t auth_head = {0};
    ret = bk_flash_read_bytes(unisound_partition->partition_start_addr, (uint8_t *)&auth_head, sizeof(auth_code_head_t));
    if (ret != BK_OK) {
        os_printf("bk_flash_read_bytes failed, ret: %d\n", ret);
        return NULL;
    }
    if (auth_head.type != UNISOUND_AUTH_CODE_TAB) {
        os_printf("auth code NULL: %d\n", auth_head.type);
        return NULL;
    }

    uint8_t * auth_data = (uint8_t *)os_malloc(auth_head.len);
    if (auth_data == NULL) {
        os_printf("no memory for auth_data.\n");
        return NULL;
    }

    if (auth_data == NULL) {
        os_printf("no memory for auth_code.\n");
        if (auth_data) {
            os_free(auth_data);
            auth_data = NULL;
        }
        return NULL;
    }
    ret = bk_flash_read_bytes(unisound_partition->partition_start_addr + sizeof(auth_code_head_t), auth_data, auth_head.len);
    if (ret != BK_OK) {
        os_printf("bk_flash_read_bytes failed, ret: %d\n", ret);
        if (auth_data) {
            os_free(auth_data);
            auth_data = NULL;
        }
        return NULL;
    }
    return auth_data;
}