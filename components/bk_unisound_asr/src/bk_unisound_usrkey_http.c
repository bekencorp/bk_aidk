#include <os/mem.h>
#include <os/str.h>

#include <components/webclient.h>
#include <components/netif.h>
#include <components/event.h>

#include <driver/hal/hal_efuse_types.h>
#include <driver/otp.h>
#include <driver/flash.h>
#include <driver/flash_partition.h>
#include <vendor_flash_partition.h>

#include "lwip/errno.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "bk_wifi.h"
#include "bk_wifi_types.h"
#include "netif/bridgeif.h"
#include "lwip/netifapi.h"
#include "lwip/inet.h"

#include "cJSON.h"

#include "bk_unisound_authcode.h"

#include "ais_lite_encrypt_data_struct.h"
#include "ais_lite_version.h"
#include "ais-lite-ual-ofa.h"
#include "ais_lite_log.h"
#include "ofa_consts.h"
	
// Define here, encryption API interface attribute is default
#define AIS_LITE_ENCRYPT_API_DEFAULT
#include "ais_lite_encrypt_main.h"

#define TAG  "usrkey_http"

#define LOG_MOD_ENCRYPT         NULL
#define LOG_MOD_SENSITIVE_INFO  NULL

#define ERR_LOCAL_RECV_HTTP_MASSAGE_FAIL BK_FAIL

beken_thread_t user_key_task_handle;

#define UNISOUND_RCV_BUF_SIZE         3072
#define UNISOUND_SEND_HEADER_SIZE     1024
#define UNISOUND_POST_DATA_MAX_SIZE   1024*2
#define UNISOUND_MAX_URL_LEN          256

static const char g_lib_type[] = "ai-kws-off";

int read_authCode_cb(char* aiCodeType, char* authCode_buf, int buf_len)
{
	BK_LOGI(TAG, "[%s:%d] tpye=%s buf_len = %d authCode_buf is %p \n", __func__,
	     __LINE__, aiCodeType, buf_len, authCode_buf);
	// No authorization code locally
	os_memset(authCode_buf, 0x00, buf_len);
	// Test if local authorization code is correct, uncomment this line if needed
	// strcpy(authCode_buf, local_auth_code);
	return 0;
}

/*
* @Description: Write authorization code (save to local, used for engine verification during next initialization)
* @Input params: aiCodeType: Engine type corresponding to the authorization code
*				 authCode_buf: Cloud authorization code save address
*				 buf_len: Cloud authorization code save buffer length (unit: bytes)
* @Output params:
*				 None
* @Return: Success: 0
*		   Failure: Other values
*/
int write_authCode_cb(char* aiCodeType, char* authCode_buf, int buf_len, char* authMsg_buf, int authMsg_buf_len)
{
	BK_LOGI(TAG, "info [%s:%d] tpye=%s buf_len = %d authCode_buf is \n%s\n", __func__, __LINE__, aiCodeType, buf_len, authCode_buf);
	BK_LOGI(TAG, "info [%s:%d] tpye=%s buf_len = %d authMsg_buf is \n%s\n", __func__, __LINE__, aiCodeType, authMsg_buf_len, authMsg_buf);
	return 0;
}

/*
 * @Description: Get timestamp (user timestamp acquisition implementation interface, used when getting timestamp, using the interface implemented by the user)
 * @Input params:
 * @Output params: long: Corresponds to time_t definition, returns current calendar time
 *				   None
 */
static time_t g_time = 0;
long get_time_cb(void)
{
/*
*	Internal implementation can be customized according to project requirements for time acquisition
*/
	g_time -= (8 * 3600);
	BK_LOGI(TAG, "\n[+]%s, time : %llu\n", __func__, g_time);
	return g_time;
}

int get_cloud_encryption_cb(char *socket_request, char *ais_lite_needle_ret)
{
    if (!socket_request) {
        BK_LOGE(TAG, "\n[-]%s, socket_request is NULL\n", __func__);
        return -1;
    }
    // Extract appKey and signature from socket_request
    char appKey[128] = {0};
    char signature[128] = {0};
    char extractedBodyData[1024] = {0};
    char contentType[128] = {0};
    char httpMethod[16] = {0};
    char host[256] = {0};
    char path[256] = {0};

    if (socket_request) {
        // Extract appKey
        char *appKeyStart = strstr(socket_request, "appKey: ");
        if (appKeyStart) {
            appKeyStart += strlen("appKey: ");
            char *appKeyEnd = strstr(appKeyStart, "\r\n");
            if (appKeyEnd) {
                strncpy(appKey, appKeyStart, appKeyEnd - appKeyStart);
                appKey[appKeyEnd - appKeyStart] = '\0';
            }
        }

        // Extract signature
        char *signatureStart = strstr(socket_request, "signature: ");
        if (signatureStart) {
            signatureStart += strlen("signature: ");
            char *signatureEnd = strstr(signatureStart, "\r\n");
            if (signatureEnd) {
                strncpy(signature, signatureStart, signatureEnd - signatureStart);
                signature[signatureEnd - signatureStart] = '\0';
            }
        }

        // Extract JSON body data
        char *jsonStart = strstr(socket_request, "{");
        if (jsonStart) {
            char *jsonEnd = strrchr(socket_request, '}');
            if (jsonEnd) {
                int jsonLen = jsonEnd - jsonStart + 1;
                if (jsonLen < sizeof(extractedBodyData)) {
                    strncpy(extractedBodyData, jsonStart, jsonLen);
                    extractedBodyData[jsonLen] = '\0';
                }
            }
        }

        // Extract Content-Type
        char *contentTypeStart = strstr(socket_request, "Content-Type: ");
        if (contentTypeStart) {
            contentTypeStart += strlen("Content-Type: ");
            char *contentTypeEnd = strstr(contentTypeStart, "\r\n");
            if (contentTypeEnd) {
                strncpy(contentType, contentTypeStart, contentTypeEnd - contentTypeStart);
                contentType[contentTypeEnd - contentTypeStart] = '\0';
            }
        }

        // Extract HTTP method and path
        char *methodStart = socket_request;
        if (methodStart) {
            char *methodEnd = strstr(methodStart, " ");
            if (methodEnd) {
                strncpy(httpMethod, methodStart, methodEnd - methodStart);
                httpMethod[methodEnd - methodStart] = '\0';
                
                // Extract path
                char *pathStart = methodEnd + 1;
                char *pathEnd = strstr(pathStart, " ");
                if (pathEnd) {
                    strncpy(path, pathStart, pathEnd - pathStart);
                    path[pathEnd - pathStart] = '\0';
                }
            }
        }

        // Extract Host
        char *hostStart = strstr(socket_request, "Host: ");
        if (hostStart) {
            hostStart += strlen("Host: ");
            char *hostEnd = strstr(hostStart, "\r\n");
            if (hostEnd) {
                strncpy(host, hostStart, hostEnd - hostStart);
                host[hostEnd - hostStart] = '\0';
            }
        }
    }
    char generate_url[256] = {0};
    int __maybe_unused url_len = 0, data_len = 0, bytes_read = 0, resp_status = 0, ret = BK_FAIL;

    struct webclient_session *session = NULL;
    /* create webclient session and set header response size */
    session = webclient_session_create(UNISOUND_SEND_HEADER_SIZE);
    if (session == NULL) {
        ret = BK_FAIL;
    }

	// Use appKey and signature extracted from socket_request
	if (appKey[0]) {
	    webclient_header_fields_add(session, "appKey: %s\r\n", appKey);
	} else {
	    webclient_header_fields_add(session, "appKey: j5gvfahot4fsrqfxun6ykr334tksqzawf4i73cia\r\n");
	}
	if (signature[0]) {
	    webclient_header_fields_add(session, "signature: %s\r\n", signature);
	} else {
	    webclient_header_fields_add(session, "signature: 00DBA3D116155E4DA0578494F723CAEACCAAD9DD\r\n");
	}
    webclient_header_fields_add(session, "Content-Length: %d\r\n", os_strlen(extractedBodyData));
    // Use Content-Type extracted from socket_request
    if (contentType[0]) {
        webclient_header_fields_add(session, "Content-Type: %s\r\n", contentType);
    } else {
        webclient_header_fields_add(session, "Content-Type: application/json\r\n");
    }

    // Construct URL using Host and Path extracted from socket_request
    if (host[0] && path[0]) {
        url_len = os_snprintf(generate_url, UNISOUND_MAX_URL_LEN, "http://%s%s", host, path);
    } else {
        url_len = os_snprintf(generate_url, UNISOUND_MAX_URL_LEN, "http://ai-off-acti.uat.hivoice.cn/rest/v1/online_device/activate/");
    }
    if ((url_len < 0) || (url_len >= UNISOUND_MAX_URL_LEN))
    {
        BK_LOGE(TAG, "URL len overflow\r\n");
        ret = BK_FAIL;
    }

    data_len = os_strlen(extractedBodyData);
    BK_LOGI(TAG, "[+]%s, body_len:%d\n", __func__, data_len);

    char *buffer = (char *) web_malloc(UNISOUND_RCV_BUF_SIZE);
    if (buffer == NULL)
    {
        ret = BK_FAIL;
        BK_LOGE(TAG, "no memory for receive response buffer.\n");
    }
    os_memset(buffer, 0x00, UNISOUND_RCV_BUF_SIZE);

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, generate_url, extractedBodyData, data_len)) != 200)
    {
        ret = BK_FAIL;
        BK_LOGE(TAG, "webclient POST request failed, response(%d) error.\n", resp_status);
    }

    BK_LOGI(TAG, "webclient post response data: \n");
    do
    {
        bytes_read = webclient_read(session, buffer, UNISOUND_RCV_BUF_SIZE);
        if (bytes_read > 0)
        {
            break;
        }
    }
    while (1);

    BK_LOGI(TAG, "bytes_read: %d\n", bytes_read);

    // Parse JSON response, extract authCode
    cJSON *root = cJSON_Parse(buffer);
    if (root != NULL) {
        cJSON *result = cJSON_GetObjectItem(root, "result");
        if (result != NULL) {
            cJSON *authCode = cJSON_GetObjectItem(result, "authCode");
            if (authCode != NULL && authCode->type == cJSON_String)
            {
                bk_err_t ret = BK_FAIL;
                ret = set_unisound_auth_code((uint8_t *)authCode->valuestring, os_strlen(authCode->valuestring)+1);
                if (ret != BK_OK) {
                    BK_LOGE(TAG, "set_unisound_auth_code failed, ret: %d\n", ret);
                }
            }
        }
        cJSON_Delete(root);
    }
    os_memcpy(ais_lite_needle_ret, buffer, bytes_read);
    return 0;
}

// Device activation information configuration
// Note: This configuration must be kept in sync with g_device_info in bk_avdk/components/bk_thirdparty/unisound/unisound_if.c
// If you modify the configuration here (e.g., APP_KEY, APP_SECRET, etc.), please synchronize the corresponding configuration in unisound_if.c
static DeviceInfo g_device_info = {
    .deviceInfo = {
        [DEVICE_INFO_APP_KEY] = "9c547203c24c41f885b52c20710aa75e1fc090ed",
        [DEVICE_INFO_APP_SECRET] = "06f1bf5da0d1444a8676793b5f5158bf",
        [DEVICE_INFO_UNIQUE_ID] = "eth0",

        [DEVICE_INFO_IMEI] = "111",
        [DEVICE_INFO_MAC] = "",
        [DEVICE_INFO_REMARK] = "33",
    },

    .ais_lite_status = 0,

    .read_authCode_cb = NULL,
    .write_authCode_cb = write_authCode_cb,
    .get_time_cb = get_time_cb,
    .get_cloud_encryption_cb = get_cloud_encryption_cb,

    .ais_lite_handle = NULL,
};

/**
 * @brief Deinitialize the user key task.
 *
 * This function deletes the user key task thread if it is currently running.
 *
 * @return BK_OK on success, or an error code on failure.
 */
static bk_err_t user_key_task_deinit(void)
{
    BK_LOGI(TAG, "%s\n", __func__);
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
    wifi_linkstate_reason_t info = {0};
    while (1)
    {
        if (check_unisound_auth_code() == BK_OK) {
            BK_LOGW(TAG, "auth code check exit already\n");
            goto exit;
        }
        bk_wifi_sta_get_linkstate_with_reason(&info);
        if (info.state != WIFI_LINKSTATE_STA_GOT_IP) {
            BK_LOGI(TAG, "waiting fot sta getting ip.\r\n");
            rtos_delay_milliseconds(5000);
        } else { 
            break;
        }
    }

    rtos_delay_milliseconds(5000);
    extern time_t ntp_sync_to_rtc(void);
    g_time = ntp_sync_to_rtc();
    BK_LOGI(TAG, "ais-lite version is %s\n", ais_lite_get_version());
    // Set ais-lite library internal log level, default level is LOG_LEVEL_WARNING
    //	ais_lite_log_level_set(LOG_LEVEL_VERBOSE);

    // Test authorization
    bk_err_t ret = ais_lite_encrypt_create(&g_device_info, g_lib_type);
    if (ret == BK_OK)
    {
        BK_LOGI(TAG, "info [%s:%d] success %d\n", __func__, __LINE__, ret);
    }
    else
    {
        BK_LOGE(TAG, "info [%s:%d] fail %d\n", __func__, __LINE__, ret);
    }

    {
		extern bk_err_t bk_aud_intf_asr_init(void);
		bk_aud_intf_asr_init();
    }
exit:
    user_key_task_deinit();
}

#if (CONFIG_SYS_CPU0)
/**
 * @brief Initialize the user key task.
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
                            1024 * 10,
                            (beken_thread_arg_t)0);
    if (ret != kNoErr) {
        BK_LOGE(TAG, "create user key task fail: %d\r\n", ret);
        user_key_task_handle = NULL;
    }
    return ret;
}
#endif


