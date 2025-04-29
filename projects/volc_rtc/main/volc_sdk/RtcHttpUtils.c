// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include "RtcHttpUtils.h"
#include <common/sys_config.h>
#include <components/log.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include "components/webclient.h"

#define TAG "RTC_HTTP"

rtc_req_result_t rtc_http_post(rtc_post_config_t* config) {
    struct webclient_session* session = NULL;
    int resp_status = -1;
    int bytes_read = 0;
    char *buffer = NULL;
    rtc_req_result_t req_result = {-1, NULL};

    if (!config || !config->uri || !config->post_data) {
        BK_LOGE(TAG, "Invalid parameters: config\r\n");
        goto __exit;
    }

	/* create webclient session and set header response size */
	session = webclient_session_create(SEND_HEADER_SIZE);
	if (session == NULL) {
        BK_LOGE(TAG, "Invalid session");
	    goto __exit;
	}

	webclient_header_fields_add(session, "Content-Length: %d\r\n", os_strlen(config->post_data));
    /*Generate https header*/
    if (config->headers) {
        int header_index = 0;
        while(config->headers[header_index]) {
            webclient_header_fields_add(session, "%s: %s\r\n", config->headers[header_index], config->headers[header_index + 1]);
            header_index += 2;
        }
    }

	buffer = (char *)psram_malloc(2048);
	if (buffer == NULL) {
		BK_LOGE(TAG,"no memory for receive response buffer\r\n");
		goto __exit;
	}
	os_memset(buffer, 0, 2048);

    /* send POST request by default header */
	if ((resp_status = webclient_post(session, config->uri, config->post_data, os_strlen(config->post_data))) != 200) {
		BK_LOGE(TAG,"webclient POST request failed, response(%d) error.\n", resp_status);
        goto __exit;
	}

    BK_LOGI(TAG,"webclient post response data: \n");

    do {
		bytes_read = webclient_read(session, buffer, 2048);
		if (bytes_read > 0)
		{
			break;
		}
	} while (1);

    req_result.code = resp_status;
    req_result.response = buffer;

__exit:
	if (session) {
		webclient_close(session);
	}

    return req_result;
}

void rtc_request_free(rtc_req_result_t *result) {
     if (result && result->response) {
        psram_free(result->response);
        result->response = NULL;
    }
}