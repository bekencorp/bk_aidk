 
/*
 * Company Name: 上海华镇电子科技有限公司
 * Project Name: Armino - ai
 * File Name: usrkey_http.c
 * Creation Date: 2025-06-05
 *
 * Description:
 * 上海华镇中文离线语音识别在线授权
 * 
 */ 

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include <components/webclient.h>
#include <components/netif.h>
#include <components/event.h>
#include <driver/otp.h>
#include <os/str.h>
#include <os/mem.h>

#include "lwip/errno.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/netifapi.h"
#include "lwip/inet.h"
#include "bk_wifi.h"
#include "bk_wifi_types.h"
#include "netif/bridgeif.h"

#include "wanson_license.h"

#define TAG  "wanson_usrkey"

#define LOGI(fmt, ...) bk_printf("["TAG "]" fmt "\n", ##__VA_ARGS__)
#define LOG_BUF(buffer, length) do { \
    for (int i = 0; i < (length); i++) { \
        BK_LOG_RAW("0x%02X ", (buffer)[i]); \
    } \
    BK_LOG_RAW("\n"); \
} while(0)

#define LOG_BUF_C(buffer, length) do { \
    for (int i = 0; i < (length); i++) { \
        BK_LOG_RAW("%c", (buffer)[i]); \
    } \
    BK_LOG_RAW("\n"); \
} while(0)

#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)

#define USER_ID "773691CC577C08DD"
//从上海华镇电子科技有限公司获取用户ID，需要通语音识别初始化使用的用户ID保持一致

beken_thread_t user_key_task_handle;
/**
 * @brief Deinitialize the user key task.
 *
 * This function deletes the user key task thread if it is currently running.
 *
 * @return BK_OK on success, or an error code on failure.
 */
static bk_err_t user_key_task_deinit(void)
{
    if (user_key_task_handle) {
        rtos_delete_thread(&user_key_task_handle);
        user_key_task_handle = NULL;
    }
    return BK_OK;
}

/**
 * @brief Task to handle user key retrieval and authorization.
 *
 * This function waits for the Wi-Fi station to obtain an IP address,
 * then retrieves the user key and performs authorization.
 */
static void user_key_task(void)
{
    wifi_linkstate_reason_t info = { 0 };
    while (1) {
        bk_wifi_sta_get_linkstate_with_reason(&info);
         if (info.state != WIFI_LINKSTATE_STA_GOT_IP)
         {
            LOGI(" waiting fot sta getting ip\r\n");
            rtos_delay_milliseconds(2000);
         } else {
             LOGI(" [+]%s, git ip.\r\n", __func__);
             break;
         }
    }

    wanson_authorization((char*) USER_ID);
    user_key_task_deinit();
}

#if (CONFIG_SYS_CPU0)
/**
 * @brief Initialize the user key task.
 * @brief 初始化上海华镇中文在线授权流程任务
 *
 * This function creates a thread to handle user key retrieval and authorization.
 * If the Wanson OTP authorization is already ready, it does not create the task.
 *
 * @return BK_OK on success, or an error code on failure.
 */
bk_err_t user_key_task_init(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_create_thread(&user_key_task_handle,
        5,
        "user_key_task",
        (beken_thread_function_t)user_key_task,
        1024 * 2,
        (beken_thread_arg_t)0);
    if (ret != kNoErr) {
        LOGE("create user key task fail: %d\r\n", ret);
        user_key_task_handle = NULL;
    }

    return ret;
}
#endif


 