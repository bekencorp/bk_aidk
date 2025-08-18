#ifndef __BK_SMART_CONFIG_SENSENOVA_ADAPTER_H__
#define __BK_SMART_CONFIG_SENSENOVA_ADAPTER_H__

enum sensenova_wss_type {
    CREATE_SESSION                    = 0,
    REQUEST_AGORA_CHANNEL_INFO        = 1,
    REQUEST_AGORA_TOKEN               = 2,
    START_SERVING                     = 3,
};

#define SENSENOVA_WSS_TXT_SIZE 200

// Sensenova WebSocket Secure URL for duplex communication
#define CONFIG_SENSENOVA_WSS_URL "wss://api-gai.sensetime.com/agent-5o/duplex/ws2"
// LLM configuration parameters for voice activity detection
#define CONFIG_SENSENOVA_LLM_CONFIG "vad_pos_threshold=0.98&vad_min_speech_duration_ms=400&vad_min_silence_duration_ms=800"
// Sensenova issuer identification
#define CONFIG_SENSENOVA_ISS "xxx"
// Sensenova secret key
#define CONFIG_SENSENOVA_SECRET "xxx"


#endif
