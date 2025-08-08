// Copyright 2020-2025 Beken
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

#include <common/sys_config.h>
#include <components/log.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include "components/webclient.h"
#include "cJSON.h"
#include "components/bk_uid.h"
#include "bk_genie_comm.h"
#include "bk_smart_config_ali_adapter.h"
#include "bk_smart_config.h"
#include "app_event.h"
#include "bk_factory_config.h"
#include "wifi_boarding_utils.h"
#include <stdio.h>
#include "lib_c_storage.h"
#include "lib_c_sdk.h"
#include "components/system.h"

#define TAG "bk_sconf"
#define C_ALIYUN_API_MMI_GET_TOKEN  "/api/token/v1/getToken"
#define C_ALIYUN_API_MMI_REGISTER   "/api/device/v1/register"
#define C_ALIYUN_HOST_MMI       "bailian.multimodalagent.aliyuncs.com"
#define C_APP_ID						"xxx"
#define C_APP_SECRET					"xxx"
extern bool first_time_for_network_reconnect;
extern void ali_auto_run(uint8_t reset);
extern int32_t _mmi_event_callback(uint32_t event, void *param);
static void bk_hex_dump(char *s, int length)
{
	   //os_printf("bk begin dump:\r\n");
		for (int i = 0; i < length; i++)
			BK_RAW_LOGI(NULL, "%c", *(u8 *)(s+i));
		BK_RAW_LOGI(NULL, "\r\n");
}

void bk_app_notify_wakeup_event(void)
{
    BK_LOGI(TAG,"%s line:%d c_mmi_start_speech\r\n", __func__, __LINE__);
    c_mmi_start_speech();
}

int bk_http_post(bk_webclient_input_t *input, char *ouput) {
    int ret = 0;
	int rx_buffer_size = input->rx_buffer_size;
	int header_size = input->header_size;
	char *uri = os_strdup(input->url);
	char *post_data = NULL;
	size_t data_len = 0;
	struct webclient_session* session = NULL;
	unsigned char *buffer = NULL;
	int bytes_read, resp_status;

	if (input->post_data) {
		post_data = input->post_data;
		data_len = strlen(post_data);
	}

	buffer = (unsigned char *) web_malloc(rx_buffer_size);
	if (buffer == NULL)
	{
		BK_LOGE(TAG,"no memory for receive response buffer.\n");
		ret = -5;
		goto __exit;
	}

	/* create webclient session and set header response size */
	session = webclient_session_create(header_size);
	if (session == NULL)
	{
		ret = -5;
		goto __exit;
	}

	/* build header for upload */
	webclient_header_fields_add(session, "Content-Length: %d\r\n", strlen(post_data));
	webclient_header_fields_add(session, "Content-Type: application/json\r\n");
	/* send POST request by default header */
	if (post_data && ((resp_status = webclient_post(session, uri, post_data, data_len)) != 200))
	{
		BK_LOGE(TAG,"webclient POST request failed, response(%d) error.\n", resp_status);
		ret = -1;
		goto __exit;
	}

	BK_LOGI(TAG,"webclient post response data: \n");
    do
    {
        bytes_read = webclient_read(session, buffer, rx_buffer_size);
        if (bytes_read > 0)
        {
            break;
        }
    }
    while (1);
    BK_LOGI(TAG, "bytes_read: %d\n", bytes_read);
    BK_LOGI(TAG, "buffer %s.\n", buffer);
    if (bytes_read > 0 && ouput) {
        memcpy(ouput, buffer, bytes_read);
        ouput[bytes_read] = '\0';
    }

__exit:
	if (session)
	{
		webclient_close(session);
	}

	if (buffer)
	{
		web_free(buffer);
	}

	if (uri)
	{
		web_free(uri);
	}

	return ret;

    BK_LOGI(TAG,"bk_http_post result(%d).\n", ret);
    return ret;
}

int bk_http_request(char *url, char *post_data, char *output)
{
	int err;

	BK_LOGI(TAG, "bk_http_request url:%s post_data:\r\n", url);
	bk_hex_dump(post_data, strlen(post_data));

	bk_webclient_input_t config = {
		.url = url,
		.header_size = 1024*2,
		.rx_buffer_size = 1024*4,
		.post_data = post_data
	};

	err = bk_http_post(&config, output);
	if(err == BK_OK){
		BK_LOGE(TAG, "bk_http_post ok\r\n");
	}
	else{
		BK_LOGE(TAG, "bk_http_post fail, err:%d\r\n", err);
	}
	return err;
}

int bk_sconf_rsp_parse(char *buffer, char *output)
{
    int code_update = 0;
    cJSON *json = cJSON_Parse(buffer);
    if (!json)
    {
        BK_LOGE(TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
        return BK_FAIL;
    }
    cJSON *code = cJSON_GetObjectItem(json, "code");
    if (code && ((code->type & 0xFF) == cJSON_Number)) {
        code_update = code->valueint;
        switch(code_update)
        {
            case HTTP_STATUS_SUCCESS:
                break;
            default:
                BK_LOGI(TAG, "code not success:%d\n", code_update);
                cJSON_Delete(json);
                return BK_FAIL;
                break;
        }
    }
    else {
        BK_LOGE(TAG, "[Error] not find code msg\n");
        cJSON_Delete(json);
        return BK_FAIL;
    }

    cJSON* data = cJSON_GetObjectItem(json, "data");

    if (data == NULL) {
        cJSON_Delete(json);
        BK_LOGE(TAG, "Not found data object.");
        return BK_FAIL;
    }
    char *data_str = cJSON_PrintUnformatted(data);
	if (output)
        os_memcpy(output, data_str, strlen(data_str));

    free(data_str);
    cJSON_Delete(json);
    return BK_OK;
}

int bk_sconf_save_agent_info(void)
{
	uint8_t base_mac[BK_MAC_ADDR_LEN] = {0};
	char str_mac[30] = {0};
	int err = bk_get_mac(base_mac, MAC_TYPE_BASE);
	if (err != BK_OK)
		return err;
	snprintf(str_mac, 30, "%02x%02x%02x%02x%02x%02x",
		base_mac[0], base_mac[1], base_mac[2], base_mac[3], base_mac[4], base_mac[5]);
    if ((err = c_storage_reset()) != BK_OK) {
		return err;
	}
    if ((err = c_storage_set_app_id_str(C_APP_ID)) != BK_OK) {
		return err;
	}
    if ((err = c_storage_set_app_secret_str(C_APP_SECRET)) != BK_OK) {
		return err;
	}
    if ((err = c_storage_set_device_name(str_mac)) != BK_OK) {
		return err;
	}
    if ((err = c_storage_save()) != BK_OK) {
		return err;
	}
	return BK_OK;
}

int ali_mmi_sdk_init(void)
{
	int err = 0;
	long long timestamp;
	char str_timestamp[64] = {0};
	char rsp[1024] = {0};
	char req[C_SDK_REQ_LEN_REGISTER];
	char *url = os_zalloc(256);
	char *rsp_data = os_zalloc(1024);
	if ((rsp_data == NULL) || (url == NULL))
	{
		BK_LOGE(TAG, "%s malloc fail:%d no mem\r\n", __func__);
		err = BK_FAIL;
		goto exit;
	}

	err = c_mmi_sdk_init();
	if (err != BK_OK)
	{
		BK_LOGE(TAG, "%s c_mmi_sdk_init fail:%d\r\n", __func__, __LINE__);
		goto exit;
	}

	if (c_storage_device_is_registered() == 0) {
		BK_LOGI(TAG, "%s c_storage_device_is_unregistered\r\n", __func__);
		if ((err = bk_sconf_save_agent_info()) != BK_OK) {
			BK_LOGE(TAG, "%s ali_mmi_save_agent_info fail %d\r\n", __func__, err);
			goto exit;
		}
		timestamp = util_get_timestamp();
		snprintf(str_timestamp, sizeof(str_timestamp), "%lld", timestamp);
	    if ((err = c_device_gen_register_req(req, str_timestamp)) != BK_OK) {
			BK_LOGE(TAG, "%s c_device_gen_register_req fail %d\r\n", __func__, err);
			goto exit;
		}
		snprintf(url, 256, "%s://%s%s", "https",
				 C_ALIYUN_HOST_MMI, C_ALIYUN_API_MMI_REGISTER);
	    if ((err = bk_http_request(url, req, rsp)) != BK_OK) {
			BK_LOGE(TAG, "%s bk_http_request fail %d\r\n", __func__, err);
			goto exit;
		}
		if ((err = bk_sconf_rsp_parse(rsp, rsp_data)) != BK_OK) {
			BK_LOGE(TAG, "%s bk_sconf_rsp_parse fail %d\r\n", __func__, err);
			goto exit;
		}
	    if ((err = c_device_analyze_register_rsp(rsp_data)) != BK_OK) {
			BK_LOGE(TAG, "%s c_device_analyze_register_rsp fail %d\r\n", __func__, err);
			goto exit;
		}
	}
	else {
		BK_LOGI(TAG, "%s c_storage_device_is_registered\r\n", __func__);
	}
	timestamp = util_get_timestamp();
	snprintf(str_timestamp, sizeof(str_timestamp), "%lld", timestamp);
	if ((err = c_mmi_gen_get_token_req(req, str_timestamp)) != BK_OK) {
		BK_LOGE(TAG, "%s c_mmi_gen_get_token_req fail %d\r\n", __func__, err);
		goto exit;
	}
	snprintf(url, 256, "%s://%s%s", "https",
			 C_ALIYUN_HOST_MMI, C_ALIYUN_API_MMI_GET_TOKEN);
	if ((err = bk_http_request(url, req, rsp)) != BK_OK) {
		BK_LOGE(TAG, "%s bk_http_request fail %d\r\n", __func__, err);
		goto exit;
	}
	if ((err = bk_sconf_rsp_parse(rsp, rsp_data)) != BK_OK) {
		BK_LOGE(TAG, "%s bk_sconf_rsp_parse fail %d\r\n", __func__, err);
		goto exit;
	}
	if ((err = c_mmi_analyze_get_token_rsp(rsp_data)) != BK_OK) {
		BK_LOGE(TAG, "%s c_mmi_analyze_get_token_rsp fail %d\r\n", __func__, err);
		goto exit;
	}

exit:
	if (rsp_data)
		os_free(rsp_data);
	if (url)
		os_free(url);
	return err;
}

int bk_sconf_wakeup_agent(uint8_t reset)
{
	int err;
	if ((err = c_mmi_register_event_callback(_mmi_event_callback)) != BK_OK) {
		BK_LOGE(TAG, "%s, c_mmi_register_event_callback fail\n", __func__);
		return err;
	}
	if ((err = ali_mmi_sdk_init()) != BK_OK) {
		BK_LOGE(TAG, "%s, ali_mmi_sdk_init fail\n", __func__);
		return err;
	}
	return BK_OK;
}

void bk_sconf_start_ali_wss(uint8_t reset)
{
    ali_auto_run(reset);
}

void bk_sconf_trans_start(void)
{
    if (first_time_for_network_reconnect) {
        first_time_for_network_reconnect = false;
        bk_sconf_start_ali_wss(1);
    } else
        bk_sconf_start_ali_wss(0);
}

void bk_sconf_trans_stop(void)
{

}

int bk_sconf_post_nfc_id(uint8_t *nfc_id)
{
    return 0;
}

void bk_sconf_begin_to_switch_ir_mode(void)
{
}

uint16_t bk_sconf_send_agent_info(char *payload, uint16_t max_len)
{
    unsigned char uid[32] = {0};
    char uid_str[65] = {0};
    uint16 len = 0;

    bk_uid_get_data(uid);
    for (int i = 0; i < 24; i++)
    {
        sprintf(uid_str + i * 2, "%02x", uid[i]);
    }
    len = os_snprintf(payload, max_len, "{\"channel\":\"%s\"}", uid_str);
    BK_LOGI(TAG, "ori channel name:%s, %d\r\n", uid_str, len);
    return len;
}

void  bk_sconf_prase_agent_info(char *payload, uint8_t reset)
{
    BK_LOGI(TAG, "%s, begin ali_auto_run\n", __func__);
    if (!bk_sconf_is_net_pan_configured())
    {
        app_event_send_msg(APP_EVT_CLOSE_BLUETOOTH, 0);
    }
    bk_sconf_sync_flash();
    ali_auto_run(reset);
}
