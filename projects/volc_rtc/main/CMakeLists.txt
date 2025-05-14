set(incs . include)
set(srcs)

list(APPEND srcs
    app_main.c
)

if (CONFIG_SYS_CPU0)

if (CONFIG_NETWORK_AUTO_RECONNECT)
list(APPEND srcs
    smart_config/bk_genie_smart_config.c
)
endif()


list(APPEND incs
            volc_sdk
)

list(APPEND srcs
    volc_sdk/volc_rtc_trigger.c
    volc_sdk/volc_rtc.c
)

if (CONFIG_HTTP_REQUEST_AGENT)
list(APPEND srcs
    volc_sdk/RtcBotUtils.c
    volc_sdk/RtcHttpUtils.c
)
endif()

endif()

if (CONFIG_SYS_CPU1)
list(APPEND srcs

)
endif()



if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()


armino_component_register(SRCS "${srcs}" 
    INCLUDE_DIRS "${incs}" 
    PRIV_REQUIRES bk_init lwip_intf_v2_1 VolcEngineRTCLite media_service multimedia avdk_utils bk_genie_core
                  mbedtls json lvgl bk_vfs wanson bk_wanson_asr bk_factory_config display_service ota bk_nfc)

