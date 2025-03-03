set(incs . include asr)
set(srcs)

list(APPEND srcs
    app_main.c
)

if (CONFIG_SYS_CPU0)
list(APPEND srcs
    agora_rtc_demo.c
    agora_sdk/agora_rtc.c
    audio/audio_transfer.c
    boarding_service/boarding_core.c
    boarding_service/boarding_service.c
    boarding_service/wifi_boarding_utils.c
)
endif()

list(APPEND srcs
    lvgl/lvgl_app.c
)

if (CONFIG_LVGL)
list(APPEND srcs
    lvgl/lv_example_meter.c
)
endif()

if (CONFIG_SYS_CPU2)
list(APPEND srcs
    asr/armino_asr.c
)
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()


armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES bk_init lwip_intf_v2_1 agora-iot-sdk media_service multimedia avdk_utils mbedtls json lvgl bk_vfs wanson bk_wanson_asr)
