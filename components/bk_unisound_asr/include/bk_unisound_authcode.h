#ifndef BK_UNISOUND_AUTHCODE_H_
#define BK_UNISOUND_AUTHCODE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 授权码表类型定义
 */
#define UNISOUND_AUTH_CODE_TAB 0x5B5B5B5B

/**
 * @brief 授权码最大长度定义
 */
#define AUTH_CODE_MAX_SIZE 4096

/**
 * @brief 授权码头部结构体
 */
typedef struct {
    uint32_t type;  /**< 授权码类型标识 */
    uint32_t len;   /**< 授权码数据长度 */
} auth_code_head_t;

/**
 * @brief 授权码结构体
 */
typedef struct {
    auth_code_head_t head;          /**< 授权码头部信息 */
    uint8_t *auth_code;  /**< 授权码数据缓冲区 */
} auth_code_t;


/**
 * @brief 检查当前系统是否存在有效授权码
 * @return BK_OK表示存在有效授权码，BK_FAIL表示不存在或无效
 */
bk_err_t check_unisound_auth_code(void);

/**
 * @brief 设置当前系统的授权码
 * @param authcode 授权码字符串指针
 * @param authcode_len 授权码字符串长度
 * @return BK_OK表示成功，其他值表示失败
 */
bk_err_t set_unisound_auth_code(uint8_t * authcode, uint32_t authcode_len);


/**
 * @brief 获取当前系统的授权码
 * @return 授权码字符串指针
 */
uint8_t *get_unisound_auth_code(void);

#ifdef __cplusplus
}
#endif

#endif  // BK_UNISOUND_AUTHCODE_H_
