#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <modules/audio_process.h>

// hardware speaker has two version, the new black speaker box is set to 1, else set  HARDWARE_SPEAKER_VER to 0 in kconfig.projbuild
/// customer eq parameter

#define EQ0 1
#define EQ0A0   -1668050
#define EQ0A1    734106
#define EQ0B0    934084
#define EQ0B1   -1715174
#define EQ0B2    801474
#define EQ0FREQ  0x44160000
#define EQ0GAIN  0xc1700000
#define EQ0QVAL  0x3f800000
#define EQ0FTYPE 0x01

#define EQ1 1
#define EQ1A0    -1764715
#define EQ1A1     784980
#define EQ1B0     1034243
#define EQ1B1    -1764715
#define EQ1B2     799312
#define EQ1FREQ   0x442f0000
#define EQ1GAIN   0xbf800000
#define EQ1QVAL   0x3f800000
#define EQ1FTYPE  0x0

#define EQSAMP   0x3e80
#define EQGAIN   0x4000
#define EQFGAIN  0x00000000

#define FILTER_PREGAIN_FRA_BITS (14)

#define CUST_EQ_PARA_DL_VOICE()                   \
    {                                        \
    .eq_en = 1,                                                                 \
    .filters = 2,                                                               \
    .globle_gain = (uint32_t)(1.12f * (1 << FILTER_PREGAIN_FRA_BITS)),          \
    .eq_para[0].a[0] = -EQ0A0,                                                  \
    .eq_para[0].a[1] = -EQ0A1,                                                  \
    .eq_para[0].b[0] = EQ0B0,                                                   \
    .eq_para[0].b[1] = EQ0B1,                                                   \
    .eq_para[0].b[2] = EQ0B2,                                                   \
    .eq_para[1].a[0] = -EQ1A0,                                                  \
    .eq_para[1].a[1] = -EQ1A1,                                                  \
    .eq_para[1].b[0] = EQ1B0,                                                   \
    .eq_para[1].b[1] = EQ1B1,                                                   \
    .eq_para[1].b[2] = EQ1B2,                                                   \
    .eq_load.f_gain     = EQFGAIN,                                                   \
    .eq_load.samplerate = EQSAMP,                                                    \
    .eq_load.eq_load_para[0].freq   = EQ0FREQ,                                               \
    .eq_load.eq_load_para[0].gain   = EQ0GAIN,                                               \
    .eq_load.eq_load_para[0].q_val  = EQ0QVAL,                                               \
    .eq_load.eq_load_para[0].type   = EQ0FTYPE,                                              \
    .eq_load.eq_load_para[0].enable  = EQ0,                                                  \
    .eq_load.eq_load_para[1].freq   = EQ1FREQ,                                               \
    .eq_load.eq_load_para[1].gain   = EQ1GAIN,                                               \
    .eq_load.eq_load_para[1].q_val  = EQ1QVAL,                                               \
    .eq_load.eq_load_para[1].type   = EQ1FTYPE,                                              \
    .eq_load.eq_load_para[1].enable  = EQ1,                                                  \
}

#define CUST_EQ_PARA_UL_VOICE()                                              \
    {                                        \
    .eq_en = 1,                                                                 \
    .filters = 2,                                                               \
    .globle_gain = (uint32_t)(1.12f * (1 << FILTER_PREGAIN_FRA_BITS)),          \
    .eq_para[0].a[0] = -EQ0A0,                                                  \
    .eq_para[0].a[1] = -EQ0A1,                                                  \
    .eq_para[0].b[0] = EQ0B0,                                                   \
    .eq_para[0].b[1] = EQ0B1,                                                   \
    .eq_para[0].b[2] = EQ0B2,                                                   \
    .eq_para[1].a[0] = -EQ1A0,                                                  \
    .eq_para[1].a[1] = -EQ1A1,                                                  \
    .eq_para[1].b[0] = EQ1B0,                                                   \
    .eq_para[1].b[1] = EQ1B1,                                                   \
    .eq_para[1].b[2] = EQ1B2,                                                   \
}

#if CONFIG_AEC_VERSION_V3
#define CUST_AEC_CONFIG_VOICE()                                              \
    {                                                                        \
    .aec_enable = 1,                                                              \
    .init_flags = 0x1f,                                                              \
    .ec_filter = 0x7,                                                         \
    .ec_depth = 0x2,                                                         \
    .mic_delay = 16,                                                          \
    .drc_gain = 0,                                                            \
    .voice_vol = 0xe,                                                       \
    .ref_scale = 0,                                                          \
    .ns_level = 0x5,                                                          \
    .ns_para = 0x2,                                                            \
    .ns_filter = 0x7,                                                          \
    .ns_type = NS_TRADITION,                                                             \
    .vad_enable = 0,                                                             \
    .vad_start_threshold = 300,                                               \
    .vad_stop_threshold = 960,                                                  \
    .vad_silence_threshold = 320,                                             \
    .vad_eng_threshold =2000,                                                  \
    .dual_mic_enable = 0,                                                         \
    .dual_mic_distance = 21,                                                       \
}
#else
#define CUST_AEC_CONFIG_VOICE()                                              \
    {                                                                        \
    .aec_enable = 1,                                                              \
    .init_flags = 0x1d,                                                              \
    .ec_filter = 0x7,                                                         \
    .ec_depth = 0x2,                                                         \
    .mic_delay = 16,                                                          \
    .drc_gain = 0,                                                            \
    .voice_vol = 0xd,                                                       \
    .ref_scale = 0,                                                          \
    .ns_level = 0x5,                                                          \
    .ns_para = 0x2,                                                            \
    .ns_filter = 0x3,                                                          \
    .ns_type = NS_CLOSE,                                                             \
    .vad_enable = 0,                                                             \
    .vad_start_threshold = 480,                                               \
    .vad_stop_threshold = 960,                                                  \
    .vad_silence_threshold = 320,                                             \
    .vad_eng_threshold =2000,                                                  \
    .dual_mic_enable = 0,                                                         \
    .dual_mic_distance = 21,                                                       \
}
#endif


#define CUST_SYS_CONFIG_VOICE()                                               \
    {                                                                        \
    .mic0_digital_gain=0x30,                                                  \
    .mic0_analog_gain=0x8,                                                   \
    .mic1_analog_gain=0x0,                                                   \
    .speaker_chan0_digital_gain = 0x20,                                      \
    .speaker_chan0_analog_gain = 0xA,                                         \
    .main_mic_select = 0,                                                      \
    .dmic_enable = 0,                                                          \
    .mic_mode = 0,                                                             \
    .spk_mode = 0,                                                             \
    .mic_vbias = 0,                                                            \
}


app_aud_para_t app_aud_cust_para = {
    .sys_config_voice = CUST_SYS_CONFIG_VOICE(),
    .eq_dl_voice = CUST_EQ_PARA_DL_VOICE(),
    .eq_ul_voice = CUST_EQ_PARA_UL_VOICE(),
    .aec_config_voice = CUST_AEC_CONFIG_VOICE(),
};

app_aud_para_t * get_app_aud_cust_para(void)
{
    return &app_aud_cust_para;
}

