set(incs . common)
set(srcs "")


if (CONFIG_SYS_CPU0)
list(APPEND srcs
    network_transfer.c
)
endif()


if (CONFIG_AGORA_IOT_SDK)
list(APPEND incs
            agora_rtc
)

list(APPEND srcs
	agora_rtc/agora_debug.c
	agora_rtc/agora_rtc_main.c
	agora_rtc/agora_rtc.c
)
endif()

if (CONFIG_BK_WSS_TRANS)
list(APPEND incs
            bk_wss
)

list(APPEND srcs
	bk_wss/bk_wss.c
	bk_wss/bk_wss_main.c
	bk_wss/bk_wss_debug.c
)
endif()

if (CONFIG_VOLC_RTC_EN)
list(APPEND incs
            volc_rtc
)

list(APPEND srcs
    volc_rtc/volc_rtc_main.c
    volc_rtc/volc_rtc.c
)

if (CONFIG_BK_DEV_STARTUP_AGENT)
list(APPEND srcs
    volc_rtc/RtcBotUtils.c
    volc_rtc/RtcHttpUtils.c
)
endif()

endif()

if (CONFIG_LINGXIN_AI_EN)
list(APPEND srcs
    lingxin_wss/lingxin_wss_main.c
)
endif()

armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES bk_common bk_boarding_service bk_smart_config bk_init lwip_intf_v2_1 mbedtls psa_mbedtls json multimedia avdk_utils 
                  VolcEngineRTCLite agora-iot-sdk video_engine audio_engine bk_app_event bk_factory_config LingXin_AI_IoT_VoiceChatEngine
)
