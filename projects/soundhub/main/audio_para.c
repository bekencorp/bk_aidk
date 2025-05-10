#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <modules/audio_process.h>

// hardware speaker has two version, the new black speaker box is set to 1, else set  HARDWARE_SPEAKER_VER to 0 in kconfig.projbuild
/// customer eq parameter
#if (CONFIG_HARDWARE_SPEAKER_VER == 1)

#define EQ0 1
#define EQ0A0 -1668050
#define EQ0A1 734106
#define EQ0B0 934084
#define EQ0B1 -1715174
#define EQ0B2 801474

#define EQ1 1
#define EQ1A0 -1764715
#define EQ1A1 784980
#define EQ1B0 1034243
#define EQ1B1 -1764715
#define EQ1B2 799312

#else

#define EQ0 1
#define EQ0A0 -1668050
#define EQ0A1 734106
#define EQ0B0 934084
#define EQ0B1 -1715174
#define EQ0B2 801474

#define EQ1 1
#define EQ1A0 -1764715
#define EQ1A1 784980
#define EQ1B0 1034243
#define EQ1B1 -1764715
#define EQ1B2 799312


#endif

#define FILTER_PREGAIN_FRA_BITS (14)

#define CUST_EQ_PARA_DL_VOICE()                   \
    {                                        \
    .eq_en = 1,                                                              \
    .filters = 2,                                                             \
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

#define CUST_EQ_PARA_UL_VOICE()                                              \
    {                                                                        \
    .eq_en = 1,                                                              \
    .filters = 2,                                                             \
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
    .init_flags = 0x1d,                                                              \
    .ec_filter = 0x7,                                                         \
    .ec_depth = 0x2,                                                         \
    .mic_delay = 16,                                                          \
    .drc_gain = 0,                                                            \
    .voice_vol = 0xd,                                                       \
    .ref_scale = 0,                                                          \
    .ns_level = 0x5,                                                          \
    .ns_para = 0x2,                                                            \
    .ns_filter = 0x80,                                                          \
    .ai_ns_enable = 0,                                                             \
    .vad_enable = 0,                                                             \
    .vad_start_threshold = 480,                                               \
    .vad_stop_threshold = 960,                                                  \
    .vad_silence_threshold = 320,                                             \
    .vad_eng_threshold =2000,                                                  \
    .dual_mic_enable = 0,                                                         \
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
    .ai_ns_enable = 0,                                                             \
    .vad_enable = 0,                                                             \
    .vad_start_threshold = 480,                                               \
    .vad_stop_threshold = 960,                                                  \
    .vad_silence_threshold = 320,                                             \
    .vad_eng_threshold =2000,                                                  \
    .dual_mic_enable = 0,                                                         \
}
#endif


#define CUST_SYS_CONFIG_VOICE()                                               \
    {                                                                        \
    .mic0_digital_gain=0x30,                                                  \
    .mic0_analog_gain=0x8,                                                   \
    .mic1_analog_gain=0x0,                                                   \
    .speaker_chan0_digital_gain = 0x1E,                                      \
    .speaker_chan0_analog_gain = 0xF,                                         \
    .main_mic_select = 0,                                                      \
    .dual_mic_enable = 0,                                                      \
    .dmic_enable = 0,                                                          \
}


app_aud_para_t app_aud_cust_para = {
    .sys_config_voice = CUST_SYS_CONFIG_VOICE(),
    .eq_dl_voice = CUST_EQ_PARA_DL_VOICE(),
    .eq_ul_voice = CUST_EQ_PARA_UL_VOICE(),
    .aec_config_voice = CUST_AEC_CONFIG_VOICE(),
};
