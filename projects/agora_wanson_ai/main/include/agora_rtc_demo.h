#pragma once

//#include "agora_rtc_api.h"
#include <driver/audio_ring_buff.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
    RingBufferContext           aud_tx_rb;
    int8_t                     *tx_ring_buff;
    uint32_t                    tx_size;

    RingBufferContext           aud_rx_rb;
    int8_t                     *rx_ring_buff;
    uint32_t                    rx_size;
} audio_rb_ctx_t;

#ifdef __cplusplus
}
#endif
