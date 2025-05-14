/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#ifndef __AUDIO_TRANSFER_H__
#define __AUDIO_TRANSFER_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_USE_G722_CODEC)  //G722
#if (CONFIG_G722_CODEC_RUN_ON_CPU1)
#define MIC_FRAME_SIZE   (160)
#define SEND_FRAME_SIZE   MIC_FRAME_SIZE
#endif
#if (CONFIG_G722_CODEC_RUN_ON_CPU0)
#define MIC_FRAME_SIZE   (640)
#define SEND_FRAME_SIZE   (MIC_FRAME_SIZE * (CONFIG_AUDIO_FRAME_DURATION_MS / 20))
#endif
//#elif defined(CONFIG_USE_G711U_CODEC) || defined(CONFIG_USE_G711A_CODEC)
//#define MIC_FRAME_SIZE     160
#elif defined(CONFIG_USE_OPUS_CODEC)  // OPUS
#define MIC_FRAME_SIZE   320
#define SEND_FRAME_SIZE   MIC_FRAME_SIZE
#else
#define MIC_FRAME_SIZE   160
#define SEND_FRAME_SIZE   MIC_FRAME_SIZE
#endif

#define MIC_FRAME_NUM 4

#if CONFIG_AUD_INTF_SUPPORT_G722 || CONFIG_AUD_INTF_SUPPORT_OPUS
#define AUDIO_SAMP_RATE         (16000)
#else
#define AUDIO_SAMP_RATE         (8000)
#endif

#define AEC_ENABLE              (1)


//#define CONFIG_AUDIO_ONLY
#if CONFIG_AUD_INTF_SUPPORT_G722
#define CONFIG_USE_G722_CODEC 1
#elif CONFIG_AUD_INTF_SUPPORT_OPUS
#define CONFIG_USE_OPUS_CODEC 1
#else
//#define CONFIG_USE_G711U_CODEC
//#define CONFIG_USE_G711A_CODEC
#endif

//#define CONFIG_UVC_CAMERA  /* config CONFIG_USB_UVC in cp1 */
//#define CONFIG_DVP_CAMERA

#define SPK_GAIN_MAX        (0X15)
#define SPK_VOLUME_LEVEL (11) //[0,10]

#define BANDWIDTH_ESTIMATE_MIN_BITRATE   (500000)
#define BANDWIDTH_ESTIMATE_MAX_BITRATE   (2000000)
#define BANDWIDTH_ESTIMATE_START_BITRATE (800000)

#if defined(CONFIG_USE_G711U_CODEC)   //G711U
#define CONFIG_AUDIO_CODEC_TYPE         AUDIO_CODEC_DISABLED
#define CONFIG_PCM_FRAME_LEN            320
#define CONFIG_PCM_SAMPLE_RATE          8000
#define CONFIG_PCM_CHANNEL_NUM          1
#define CONFIG_SEND_PCM_DATA

#elif defined(CONFIG_USE_G711A_CODEC) // G711A
#define CONFIG_AUDIO_CODEC_TYPE         AUDIO_CODEC_DISABLED
#define CONFIG_PCM_FRAME_LEN            320
#define CONFIG_PCM_SAMPLE_RATE          8000
#define CONFIG_PCM_CHANNEL_NUM          1
#define CONFIG_SEND_PCM_DATA

#elif defined(CONFIG_USE_G722_CODEC)  // G722
#if (CONFIG_G722_CODEC_RUN_ON_CPU1)
#define CONFIG_AUDIO_CODEC_TYPE         AUDIO_CODEC_DISABLED
#define CONFIG_PCM_FRAME_LEN            640
#define CONFIG_PCM_SAMPLE_RATE          16000
#define CONFIG_PCM_CHANNEL_NUM          1
#define CONFIG_SEND_PCM_DATA
#endif //CONFIG_G722_CODEC_RUN_ON_CPU1
#if (CONFIG_G722_CODEC_RUN_ON_CPU0)
#define CONFIG_AUDIO_CODEC_TYPE         AUDIO_CODEC_TYPE_G722
#define CONFIG_PCM_FRAME_LEN            640
#define CONFIG_PCM_SAMPLE_RATE          16000
#define CONFIG_PCM_CHANNEL_NUM          1
#define CONFIG_SEND_PCM_DATA
#endif //CONFIG_G722_CODEC_RUN_ON_CPU0

#elif defined(CONFIG_USE_OPUS_CODEC)  // OPUS
#define CONFIG_AUDIO_CODEC_TYPE         AUDIO_CODEC_DISABLED
#define CONFIG_PCM_FRAME_LEN            640
#define CONFIG_PCM_SAMPLE_RATE          16000
#define CONFIG_PCM_CHANNEL_NUM          1
#define CONFIG_SEND_PCM_DATA

#else                                // DISABLE
#define CONFIG_AUDIO_CODEC_TYPE         AUDIO_CODEC_DISABLED
#define CONFIG_PCM_FRAME_LEN            160
#define CONFIG_PCM_SAMPLE_RATE          8000
#define CONFIG_PCM_CHANNEL_NUM          1
// #define CONFIG_SEND_PCM_DATA
#endif

#if CONFIG_G722_CODEC_RUN_ON_CPU0
#define CONFIG_AUDIO_FRAME_DURATION_MS     60  // except OPUS
#else
#define CONFIG_AUDIO_FRAME_DURATION_MS     20  // except OPUS
#endif



typedef int (*user_audio_tx_data_func)(unsigned char *data, unsigned int size);
typedef enum
{
    AUD_TRAS_TX_DATA = 0,

    AUD_TRAS_EXIT,
} aud_tras_op_t;

typedef struct
{
    aud_tras_op_t op;
    uint16_t len;
} aud_tras_msg_t;


/* API */
int audio_tras_user_audio_rx_data_handle(unsigned char *data, unsigned int size);
void audio_tras_register_tx_data_func(user_audio_tx_data_func func);
bk_err_t audio_turn_on(void);
bk_err_t audio_turn_off(void);

#ifdef __cplusplus
}
#endif
#endif /* __AUDIO_TRANSFER_H__ */
