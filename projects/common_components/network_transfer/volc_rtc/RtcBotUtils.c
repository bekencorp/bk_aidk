// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include <common/sys_config.h>
#include <components/log.h>
#include <modules/wifi.h>
#include <components/netif.h>
#include <components/event.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include "cJSON.h"
#include "components/bk_uid.h"
#include "RtcBotUtils.h"
#include "RtcHttpUtils.h"
#include "volc_config.h"

#define TAG "RTC_BOT"

const char* common_headers[] = {
    "Content-Type", "application/json",
    "Authorization", "af78e30" DEFAULT_RTC_APP_ID,
    NULL
};

int start_voice_bot(rtc_room_info_t* room_info) {
    static int cjson_init_hook = 0;
    if (cjson_init_hook == 0) {
        // cJSON_Hooks hook = {
        //     .malloc_fn = impl_malloc_fn,
        //     .free_fn = impl_free_fn,
        // };
        // cJSON_InitHooks(&hook);
        cjson_init_hook = 1;
    }

    char post_data[512];
    cJSON *post_jobj = cJSON_CreateObject();
    cJSON_AddStringToObject(post_jobj, "end_point_id", DEFAULT_END_POINT_ID);
    cJSON_AddStringToObject(post_jobj, "voice_type", DEFAULT_VOICE_TYPE);
    cJSON_AddStringToObject(post_jobj, "audio_codec", "OPUS");
    cJSON_AddNumberToObject(post_jobj, "asr_type", 1);
    //cJSON_AddBoolToObject(post_jobj, "vision_enable", true);
    //cJSON_AddNumberToObject(post_jobj, "image_height", 480);
    //cJSON_AddStringToObject(post_jobj, "image_detail", "high");
    const char* json_str = cJSON_Print(post_jobj);
    strcpy(post_data, json_str);
    cJSON_Delete(post_jobj);

    rtc_post_config_t post_config = {
        .uri = "http://" DEFAULT_SERVER_HOST "/startvoicechat",
        .headers = common_headers,
        .post_data = post_data  // 根据需要传入智能体id和音色id
    };
    rtc_req_result_t post_result = rtc_http_post(&post_config);
    if (post_result.code == 200 && post_result.response != NULL) {
        // parse json
        cJSON* root = cJSON_Parse(post_result.response);
        rtc_request_free(&post_result);
        if (root == NULL) {
            BK_LOGE(TAG, "Error parsing JSON");
            return -1;
        }
        
        cJSON* data = cJSON_GetObjectItem(root, "data");

        if (data == NULL) {
            cJSON_Delete(root);
            BK_LOGE(TAG, "Not found data object.");
            return -1;
        }
        
        cJSON* app_id_item = cJSON_GetObjectItem(data, "app_id");
        const char* app_id = cJSON_GetStringValue(app_id_item);
        strcpy(room_info->app_id, app_id);

        cJSON* uid_item = cJSON_GetObjectItem(data, "uid");
        const char* uid = cJSON_GetStringValue(uid_item);
        strcpy(room_info->uid, uid);
        
        cJSON* room_id_item = cJSON_GetObjectItem(data, "room_id");
        const char* room_id = cJSON_GetStringValue(room_id_item);
        strcpy(room_info->room_id, room_id);

        cJSON* token_item = cJSON_GetObjectItem(data, "token");
        const char* token = cJSON_GetStringValue(token_item);
        strcpy(room_info->token, token);
        
        cJSON_Delete(root);

        return 200;
    } else {
        cJSON* root = cJSON_Parse(post_result.response);
        if (root != NULL) {
            cJSON* message_item = cJSON_GetObjectItem(root, "message");
            const char* message = cJSON_GetStringValue(message_item);
            BK_LOGE(TAG, "Error: %s", message);
            cJSON_Delete(root);
        }
        return post_result.code;
    }
}

int stop_voice_bot(const rtc_room_info_t* room_info) {

    char post_data[512];
    cJSON *post_jobj = cJSON_CreateObject();
    cJSON_AddStringToObject(post_jobj, "app_id", room_info->app_id);
    cJSON_AddStringToObject(post_jobj, "room_id", room_info->room_id);
    cJSON_AddStringToObject(post_jobj, "uid", room_info->uid);
    
    const char* json_str = cJSON_Print(post_jobj);
    strcpy(post_data, json_str);
    cJSON_Delete(post_jobj);
    
    rtc_post_config_t post_config = {
        .uri = "http://" DEFAULT_SERVER_HOST "/stopvoicechat",
        .headers = common_headers,
        .post_data = post_data
    };
    rtc_req_result_t post_result = rtc_http_post(&post_config);
    if (post_result.code == 200 && post_result.response != NULL) {
        // parse json
        cJSON* root = cJSON_Parse(post_result.response);
        rtc_request_free(&post_result);
        if (root == NULL) {
            BK_LOGE(TAG, "Error parsing JSON");
            return -1;
        }
        
        cJSON* data = cJSON_GetObjectItem(root, "data");

        if (data == NULL) {
            cJSON_Delete(root);
            BK_LOGE(TAG, "Not found data object.");
            return -1;
        }
        
        
        cJSON_Delete(root);

        return 200;
    } else {
        cJSON* root = cJSON_Parse(post_result.response);
        if (root != NULL) {
            cJSON* message_item = cJSON_GetObjectItem(root, "message");
            const char* message = cJSON_GetStringValue(message_item);
            BK_LOGE(TAG, "Error: %s", message);
            cJSON_Delete(root);
        }
        return post_result.code;
    }
}

int update_voice_bot(const rtc_room_info_t* room_info, const char* command, const char* message) {
    char post_data[1024];
    cJSON *post_jobj = cJSON_CreateObject();
    cJSON_AddStringToObject(post_jobj, "app_id", room_info->app_id);
    cJSON_AddStringToObject(post_jobj, "room_id", room_info->room_id);
    cJSON_AddStringToObject(post_jobj, "uid", room_info->uid);
    cJSON_AddStringToObject(post_jobj, "command", command);
    if (message) {
        cJSON_AddStringToObject(post_jobj, "message", message);
    }
    
    const char* json_str = cJSON_Print(post_jobj);
    strcpy(post_data, json_str);
    cJSON_Delete(post_jobj);

    
    rtc_post_config_t post_config = {
        .uri = "http://" DEFAULT_SERVER_HOST "/updatevoicechat",
        .headers = common_headers,
        .post_data = post_data
    };
    rtc_req_result_t post_result = rtc_http_post(&post_config);
    if (post_result.code == 200 && post_result.response != NULL) {
        // parse json
        cJSON* root = cJSON_Parse(post_result.response);
        rtc_request_free(&post_result);
        if (root == NULL) {
            BK_LOGE(TAG, "Error parsing JSON");
            return -1;
        }
        
        cJSON* data = cJSON_GetObjectItem(root, "data");

        if (data == NULL) {
            cJSON_Delete(root);
            BK_LOGE(TAG, "Not found data object.");
            return -1;
        }
        
        cJSON_Delete(root);
        return 200;
    } else {
        cJSON* root = cJSON_Parse(post_result.response);
        if (root != NULL) {
            cJSON* message_item = cJSON_GetObjectItem(root, "message");
            const char* message = cJSON_GetStringValue(message_item);
            BK_LOGE(TAG, "Error: %s", message);
            cJSON_Delete(root);
        }
        return post_result.code;
    }

}

int interrupt_voice_bot(const rtc_room_info_t* room_info) {
    return update_voice_bot(room_info, "interrupt", NULL);
}

int voice_bot_function_calling(const rtc_room_info_t* room_info, const char* message) {
    return update_voice_bot(room_info, "function", message);
}