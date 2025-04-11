set(incs . include asr bt_pan)
set(srcs)

if (CONFIG_BEKEN_GENIE_CORE)

list(APPEND srcs
    bk_genie_main.c
)

if (CONFIG_SYS_CPU0)
list(APPEND srcs
    app_event.c
    audio/audio_transfer.c
    boarding_service/boarding_core.c
    boarding_service/boarding_service.c
    boarding_service/wifi_boarding_utils.c
    led_blink.c
    countdown.c
    motor.c
)

if (CONFIG_NETWORK_AUTO_RECONNECT)
list(APPEND srcs
   smart_config/bk_genie_smart_config.c
)
endif()

if (CONFIG_NET_PAN)
list(APPEND srcs
    bt_pan/bt_manager.c
    bt_pan/pan_service.c
    bt_pan/pan_demo_cli.c
    bt_pan/storage/bluetooth_storage.c
    bt_pan/bt_comm_list.c
)
endif()

endif() #if (CONFIG_SYS_CPU0)



list(APPEND srcs
    display/avi_play.c
    display/ota_display.c
)

if (CONFIG_SYS_CPU2)
list(APPEND srcs
    asr/armino_asr.c
)
endif()

endif() # if (CONFIG_BEKEN_GENIE_CORE)

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES  lwip_intf_v2_1  media_service multimedia avdk_utils mbedtls json lvgl bk_vfs wanson bk_wanson_asr bk_factory_config display_service ota bk_nfc)
