#pragma once

#include "agora_rtc_api.h"
#include "audio_config.h"

#define CONFIG_AGORA_APP_ID "xxxxxxxxxxxxxxxxxxxxxxxxxxx" // Please replace with your own APP ID

#define CONFIG_CUSTOMER_KEY "8620fd479140455388f99420fd307363"
#define CONFIG_CUSTOMER_SECRET "492c18dcdb0a43c5bb10cc1cd217e802"

#define CONFIG_LICENSE_PID "00F8D46F55D34580ADD8A4827F822646"

// Agora Master Server URL
#define CONFIG_MASTER_SERVER_URL "https://app.agoralink-iot-cn.sd-rtn.com"

// Agora Slave Server URL
#define CONFIG_SLAVE_SERVER_URL "https://api.agora.io/agoralink/cn/api"

// Found product key form device manager platform
#define CONFIG_PRODUCT_KEY "EJIJEIm68gl5b5lI4"

#define CONFIG_CHANNEL_NAME "test-openai-av-debug"

#define CONFIG_AGORA_TOKEN "" // Please turn on token when the product is mass produced.

#define CONFIG_AGORA_UID        (123)


// (CONFIG_PCM_FRAME_LEN * 1000 / CONFIG_PCM_SAMPLE_RATE / CONFIG_PCM_CHANNEL_NUM /sizeof(int16_t))

#define DEFAULT_SDK_LOG_PATH "io.agora.rtc_sdk"