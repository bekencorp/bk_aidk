set(incs . main ../../beken_genie/main/ ../../beken_genie/main/include ../../beken_genie/main/asr ../../beken_genie/main/bt_pan)
set(srcs)

list(APPEND srcs
    ../../beken_genie/main/app_main.c
)

if (CONFIG_SYS_CPU0)
list(APPEND srcs
    ../../beken_genie/main/app_event.c
    ../../beken_genie/main/agora_rtc_demo.c
    ../../beken_genie/main/agora_sdk/agora_rtc.c
    ../../beken_genie/main/audio/audio_transfer.c
    ../../beken_genie/main/boarding_service/boarding_core.c
    ../../beken_genie/main/boarding_service/boarding_service.c
    ../../beken_genie/main/boarding_service/wifi_boarding_utils.c
    ../../beken_genie/main/agora_debug.c
    ../../beken_genie/main/led_blink.c
    ../../beken_genie/main/countdown.c
    ../../beken_genie/main/motor.c
)

if (CONFIG_NETWORK_AUTO_RECONNECT)
list(APPEND srcs
   ../../beken_genie/main/smart_config/bk_genie_smart_config.c
)
endif()

if (CONFIG_NET_PAN)
list(APPEND srcs
    ../../beken_genie/main/bt_pan/bt_manager.c
    ../../beken_genie/main/bt_pan/pan_service.c
    ../../beken_genie/main/bt_pan/pan_demo_cli.c
    ../../beken_genie/main/bt_pan/storage/bluetooth_storage.c
    ../../beken_genie/main/bt_pan/bt_comm_list.c
)
endif()
endif()

if (CONFIG_SYS_CPU1)
list(APPEND srcs
    ../../beken_genie/main/agora_debug.c
)
endif()

list(APPEND srcs
    ../../beken_genie/main/display/avi_play.c
    ../../beken_genie/main/display/ota_display.c
)

if (CONFIG_SYS_CPU2)
list(APPEND srcs
    ../../beken_genie/main/asr/armino_asr.c
)
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES bk_init lwip_intf_v2_1 agora-iot-sdk media_service multimedia avdk_utils mbedtls json lvgl bk_vfs wanson bk_wanson_asr bk_factory_config display_service ota bk_nfc)
