#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <driver/audio_ring_buff.h>
#include "audio_config.h"
#include "audio_dump_data.h"
#include "audio_transfer.h"
#include "audio_log.h"
#include <modules/audio_process.h>
#include "aud_intf.h"
#include "audio_engine.h"
#if (CONFIG_BK_WSS_TRANS || CONFIG_BK_WSS_TRANS_NOPSRAM)
#include "bk_wss.h"
#endif

#if CONFIG_SYS_CPU0 || CONFIG_SYS_CPU1
bk_err_t audio_engine_init(void)
{
    audio_dump_data_cli_init();
    return BK_OK;
}
#endif

#if CONFIG_SYS_CPU0
#define AEC_ENABLE              (1)

//static bool audio_en = false;
extern app_aud_para_t * get_app_aud_cust_para(void);
extern uint32_t volume;
extern uint32_t g_volume_gain[SPK_VOLUME_LEVEL];

audio_info_t general_audio;
static user_audio_end_func audio_notify_end = NULL;
static user_audio_end_func s_audio_tone_notify_end = NULL;

bk_err_t audio_turn_off(void)
{
    bk_err_t ret =  BK_OK;
    AUDE_LOGI("%s\n", __func__);

    /* stop voice */
    ret = bk_aud_intf_voc_stop();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        AUDE_LOGE("%s, %d, voice stop fail, ret:%d\n", __func__, __LINE__, ret);
    }

    /* deinit vioce */
    ret = bk_aud_intf_voc_deinit();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        AUDE_LOGE("%s, %d, voice deinit fail, ret:%d\n", __func__, __LINE__, ret);
    }

    bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_NULL);

    ret = bk_aud_intf_drv_deinit();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        AUDE_LOGE("%s, %d, aud_intf driver deinit fail, ret:%d\n", ret);
    }

    audio_tras_deinit();

    AUDIO_RX_SPK_DATA_DUMP_CLOSE();

    return BK_OK;
}

uint32_t audio_codec_type_mapping_str2int(char *codec_type)
{
    uint8_t aud_codec_type = AUD_INTF_VOC_DATA_TYPE_MAX;

    if(0 == os_strcmp("PCMA", codec_type))
    {
        aud_codec_type = AUD_INTF_VOC_DATA_TYPE_G711A;
    }
    else if(0 == os_strcmp("PCM", codec_type))
    {
        aud_codec_type = AUD_INTF_VOC_DATA_TYPE_PCM;
    }
    else if(0 == os_strcmp("PCMU", codec_type))
    {
        aud_codec_type = AUD_INTF_VOC_DATA_TYPE_G711U;
    }
    #if CONFIG_AUD_INTF_SUPPORT_G722
    else if(0 == os_strcmp("G722", codec_type))
    {
        aud_codec_type = AUD_INTF_VOC_DATA_TYPE_G722;
    }
    #endif
    #if CONFIG_AUD_INTF_SUPPORT_OPUS
    else if(0 == os_strcmp("OPUS", codec_type))
    {
        aud_codec_type = AUD_INTF_VOC_DATA_TYPE_OPUS;
    }
    #endif
    #if CONFIG_AUD_INTF_SUPPORT_MP3
    else if(0 == os_strcmp("MP3", codec_type))
    {
        aud_codec_type = AUD_INTF_VOC_DATA_TYPE_MP3;
    }
    #endif
    else
    {
        AUDE_LOGE("%s unknown codec type:%s \r\n", __func__,codec_type);
    }  

    return aud_codec_type;
}

char * get_name_by_codec_type(uint32_t codec_type)
{
    switch(codec_type)
    {
        case AUD_INTF_VOC_DATA_TYPE_G711A:
        {
            return "g711a";
        }
        case AUD_INTF_VOC_DATA_TYPE_PCM:
        {
            return "pcm";
        }
        case AUD_INTF_VOC_DATA_TYPE_G711U:
        {
            return "g711u";
        }
        #if CONFIG_AUD_INTF_SUPPORT_G722
        case AUD_INTF_VOC_DATA_TYPE_G722:
        {
            return "g722";
        }
        #endif
        #if CONFIG_AUD_INTF_SUPPORT_OPUS
        case AUD_INTF_VOC_DATA_TYPE_OPUS:
        {
            return "opus";
        }
        #endif
        #if CONFIG_AUD_INTF_SUPPORT_MP3
        case AUD_INTF_VOC_DATA_TYPE_MP3:
        {
            return "mp3";
        }
        #endif
        default:
        {
            AUDE_LOGE("%s invalid codec type:%d \r\n", __func__,codec_type);
            return NULL;
        }
    }
}

void bk_fill_aud_inft_info(audio_info_t *info, char *enctype, char *dectype, uint32_t adc_rate, uint32_t dac_rate, uint32_t enc_ms, uint32_t dec_ms, uint32_t enc_size, uint32_t dec_size)
{
	memset(info, 0, sizeof(audio_info_t));
	os_strcpy(info->encoding_type, enctype);
    os_strcpy(info->decoding_type, dectype);
	info->adc_samp_rate = adc_rate;
	info->dac_samp_rate = dac_rate;
	info->enc_samp_interval = enc_ms;
	info->dec_samp_interval = dec_ms;
	info->enc_node_size = enc_size;
    info->dec_node_size = dec_size;
}

void bk_get_aud_inft_info(audio_info_t *audio_info, aud_intf_voc_setup_t *aud_intf_voc_setup, char *encoder_name, char *decoder_name)
{
    bk_fill_aud_inft_info(audio_info,
                        encoder_name,
                        decoder_name,
                        aud_intf_voc_setup->aud_codec_setup_input.adc_samp_rate,
                        aud_intf_voc_setup->aud_codec_setup_input.dac_samp_rate,
                        aud_intf_voc_setup->aud_codec_setup_input.enc_frame_len_in_ms,
                        aud_intf_voc_setup->aud_codec_setup_input.dec_frame_len_in_ms,
                        bk_aud_get_enc_output_size_in_byte(),
                        bk_aud_get_dec_input_size_in_byte());
}

bk_err_t audio_codec_para_update(aud_codec_setup_input_t *input_para)
{
    bk_err_t ret = BK_ERR_AUD_INTF_OK;

    switch(input_para->encoder_type)
    {
        case AUD_INTF_VOC_DATA_TYPE_G711A:
        {
            input_para->enc_bitrate = input_para->adc_samp_rate*input_para->enc_data_depth_in_byte*8/2;//64000;//sample rate 8000,1/2 coderate
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_PCM:
        {
            input_para->enc_bitrate = input_para->adc_samp_rate*input_para->enc_data_depth_in_byte*8;//256000;//sample rate 16000,no encode
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_G711U:
        {
            input_para->enc_bitrate = input_para->adc_samp_rate*input_para->enc_data_depth_in_byte*8/2;//64000;//sample rate 8000,1/2 coderate
            break;
        }
        #if CONFIG_AUD_INTF_SUPPORT_G722
        case AUD_INTF_VOC_DATA_TYPE_G722:
        {
            input_para->enc_bitrate = 64000;//sample rate 16000,1/4 coderate
            break;
        }
        #endif
        #if CONFIG_AUD_INTF_SUPPORT_OPUS
        case AUD_INTF_VOC_DATA_TYPE_OPUS:
        {
            input_para->enc_bitrate = 16000;//sample rate 16000,1/16 coderate
            input_para->enc_vbr_en = 1;
            break;
        }
        #endif
        default:
        {
            AUDE_LOGE("%s invalid audio encoder type:%d \r\n", __func__,input_para->encoder_type);
            ret = BK_ERR_AUD_INTF_FAIL;
            break;
        }
    }

    switch(input_para->decoder_type)
    {
        case AUD_INTF_VOC_DATA_TYPE_G711A:
        {
            input_para->dec_bitrate = input_para->dac_samp_rate*input_para->dec_data_depth_in_byte*8/2;//64000;//sample rate 8000,1/2 coderate
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_PCM:
        {
            input_para->dec_bitrate = input_para->dac_samp_rate*input_para->dec_data_depth_in_byte*8;//256000;//sample rate 16000,no encode
            break;
        }
        case AUD_INTF_VOC_DATA_TYPE_G711U:
        {
            input_para->dec_bitrate = input_para->dac_samp_rate*input_para->dec_data_depth_in_byte*8/2;//64000;//sample rate 8000,1/2 coderate
            break;
        }
        #if CONFIG_AUD_INTF_SUPPORT_G722
        case AUD_INTF_VOC_DATA_TYPE_G722:
        {
            input_para->dec_bitrate = 64000;//sample rate 16000,1/4 coderate
            break;
        }
        #endif
        #if CONFIG_AUD_INTF_SUPPORT_OPUS
        case AUD_INTF_VOC_DATA_TYPE_OPUS:
        {
            input_para->dec_bitrate = 64000;//sample rate 16000,max support 1/4 coderate
            input_para->dec_vbr_en = 1;
            break;
        }
        #endif
        #if CONFIG_AUD_INTF_SUPPORT_MP3
        case AUD_INTF_VOC_DATA_TYPE_MP3:
        {
            AUDE_LOGI("%s audio decoder type:%d \r\n", __func__,input_para->decoder_type);
            break;
        }
        #endif
        default:
        {
            AUDE_LOGE("%s invalid audio decoder type:%d \r\n", __func__,input_para->decoder_type);
            ret = BK_ERR_AUD_INTF_FAIL;
            break;
        }
    }

    return ret;
}

void audio_register_play_finish_func(user_audio_end_func func)
{
	audio_notify_end = func;
}

int audio_play_data_end_notify(void *param)
{
	AUDE_LOGI("audio_play_data_end_notify\r\n");
	int rval = BK_OK;
	if (audio_notify_end)
    {
        rval = audio_notify_end(param);
    }
    else
    {
        AUDE_LOGE("audio_play_data_end_notify failed, invalid audio_tx_func\n");
        return BK_FAIL;
    }
	return rval;
}

void audio_register_tone_play_finish_func(user_audio_end_func func)
{
    if(s_audio_tone_notify_end)
	{
		AUDE_LOGW("%s cb is already reg !!! %p %p\n", __func__, s_audio_tone_notify_end, func);
	}
	s_audio_tone_notify_end = func;
}

static int tone_play_data_end_notify(void *param)
{
	AUDE_LOGI("%s\r\n", __func__);
	int rval = BK_OK;

	if (s_audio_tone_notify_end)
    {
        rval = s_audio_tone_notify_end(param);
    }
    else
    {
        AUDE_LOGE("%s failed, invalid cb\n", __func__);
        return BK_FAIL;
    }
	return rval;
}


bk_err_t audio_turn_on(void)
{
    bk_err_t ret =  BK_OK;
    char * encoder_name;
    char * decoder_name;
    
    AUDE_LOGI("%s\n", __func__);

    AUDIO_RX_SPK_DATA_DUMP_OPEN();

    aud_intf_drv_setup_t aud_intf_drv_setup = DEFAULT_AUD_INTF_DRV_SETUP_CONFIG();
    aud_intf_voc_setup_t aud_intf_voc_setup = DEFAULT_AUD_INTF_VOC_SETUP_CONFIG();

    aud_intf_voc_setup.aud_codec_setup_input.encoder_type  = audio_codec_type_mapping_str2int(CONFIG_AUDIO_ENCODER_TYPE);
    aud_intf_voc_setup.aud_codec_setup_input.decoder_type  = audio_codec_type_mapping_str2int(CONFIG_AUDIO_DECODER_TYPE);
    aud_intf_voc_setup.aud_codec_setup_input.enc_frame_len_in_ms = CONFIG_AUDIO_FRAME_DURATION_MS;
    aud_intf_voc_setup.aud_codec_setup_input.dec_frame_len_in_ms = CONFIG_AUDIO_FRAME_DURATION_MS;
    aud_intf_voc_setup.aud_codec_setup_input.adc_samp_rate = CONFIG_AUDIO_ADC_SAMP_RATE;
    aud_intf_voc_setup.aud_codec_setup_input.dac_samp_rate = CONFIG_AUDIO_DAC_SAMP_RATE;

    ret = audio_codec_para_update(&aud_intf_voc_setup.aud_codec_setup_input);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        AUDE_LOGE("%s, %d, audio_codec_para_update fail, ret:%d\n", __func__, __LINE__, ret);
    }

    aud_intf_voc_setup.spk_mode   = AUD_DAC_WORK_MODE_DIFFEN;
    aud_intf_voc_setup.aec_enable = AEC_ENABLE;
#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
    aud_intf_voc_setup.mic_gain   = 0x30;
#else
    aud_intf_voc_setup.mic_gain   = 0x3F;
#endif
    aud_intf_voc_setup.spk_gain   = g_volume_gain[volume];
    aud_intf_voc_setup.mic_type = AUD_INTF_MIC_TYPE_BOARD;
    aud_intf_voc_setup.spk_type = AUD_INTF_MIC_TYPE_BOARD;
#if CONFIG_AUD_INTF_SUPPORT_SPK_PLAY_FINISH_NOTIFY
	aud_intf_voc_setup.spk_play_finish_notify = audio_play_data_end_notify;
#endif

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE_PLAY_FINISH_NOTIFY
    aud_intf_voc_setup.prompt_tone_play_finish_notify = tone_play_data_end_notify;
#endif

    bk_aud_intf_aud_codec_init(&aud_intf_voc_setup.aud_codec_setup_input);

    audio_tras_init();
    aud_intf_drv_setup.aud_intf_tx_mic_data = send_audio_data_to_net_transfer;

    ret = bk_aud_intf_drv_init(&aud_intf_drv_setup);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        AUDE_LOGE("%s, %d, aud_intf driver init fail, ret:%d\n", __func__, __LINE__, ret);
    }

    ret = bk_aud_intf_set_mode(AUD_INTF_WORK_MODE_VOICE);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        AUDE_LOGE("%s, %d, aud_intf set_mode fail, ret:%d\n", __func__, __LINE__, ret);
    }

    app_aud_para_t * cust_aud_para = get_app_aud_cust_para();
    bk_aud_intf_audio_para_set(cust_aud_para);

    ret = bk_aud_intf_voc_init(aud_intf_voc_setup);
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        AUDE_LOGE("bk_aud_intf_voc_init fail, ret:%d \r\n", ret);
    }

    encoder_name = get_name_by_codec_type(aud_intf_voc_setup.aud_codec_setup_input.encoder_type);
    decoder_name = get_name_by_codec_type(aud_intf_voc_setup.aud_codec_setup_input.decoder_type);
    bk_get_aud_inft_info(&general_audio, &aud_intf_voc_setup,encoder_name,decoder_name);

#if CONFIG_BK_WSS_TRANS_NOPSRAM
	rtc_get_dialog_info(&aud_intf_voc_setup, encoder_name, decoder_name);
#endif

    ret = bk_aud_intf_voc_start();
    if (ret != BK_ERR_AUD_INTF_OK)
    {
        AUDE_LOGE("bk_aud_intf_voc_start fail, ret:%d \r\n", ret);
    }

    return BK_OK;
}
#endif
