set(incs . include)
set(srcs)

if (CONFIG_SYS_CPU0)
if (CONFIG_BK_SMART_CONFIG)
list(APPEND srcs
    src/core/bk_smart_config_core.c
)

if (CONFIG_AGORA_IOT_SDK)
list(APPEND srcs
	src/adapter/agora/bk_smart_config_agora_adapter.c
)
endif()

if (CONFIG_BK_WSS_TRANS)
list(APPEND srcs
	src/adapter/wss/bk_smart_config_wss_adapter.c
)
endif()

if (CONFIG_VOLC_RTC_EN)
list(APPEND srcs
	src/adapter/volc/bk_smart_config_volc_adapter.c
)
endif()

if (CONFIG_LINGXIN_AI_EN)
list(APPEND srcs
	src/adapter/lingxin/bk_smart_config_lingxin_adapter.c
)
endif()

endif()
endif()

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" 
    PRIV_REQUIRES bk_boarding_service mbedtls psa_mbedtls json bk_bt bk_factory_config bk_app_event multimedia 
                  network_transfer agora-iot-sdk video_engine audio_engine VolcEngineRTCLite)
