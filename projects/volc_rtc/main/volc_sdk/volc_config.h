// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#if CONFIG_HTTP_REQUEST_AGENT
// RTC APP ID
#define DEFAULT_RTC_APP_ID  "xxx"
// 服务端的地址
#define DEFAULT_SERVER_HOST "xxx"
// 默认的智能体id
#define DEFAULT_BOT_ID      "xxx"
// 默认声音id
#define DEFAULT_VOICE_ID    "BV007_streaming"
#else
// RTC APP ID
#define DEFAULT_RTC_APP_ID  "zzzz"
// 服务端的地址
#define DEFAULT_ROOM_ID     "zzzz"
// 默认的智能体id
#define DEFAULT_USER_ID     "zzzz"
// 默认声音id
#define DEFAULT_TOKEN       "zzzz"
#endif


// (CONFIG_PCM_FRAME_LEN * 1000 / CONFIG_PCM_SAMPLE_RATE / CONFIG_PCM_CHANNEL_NUM /sizeof(int16_t))

#define DEFAULT_SDK_LOG_PATH "io.volc.rtc_sdk"