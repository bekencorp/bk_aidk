set(incs .)
set(srcs "")

list(APPEND srcs
    app_event.c
)

set(priv_req bk_common multimedia bk_led_blink bk_countdown main bk_factory_config ota bk_boarding_service bk_smart_config network_transfer VolcEngineRTCLite audio_engine)

if (CONFIG_DUAL_SCREEN_AVI_PLAY)
list(APPEND priv_req dual_screen_avi_play)
endif()

if (CONFIG_SINGLE_SCREEN_AVI_PLAY)
list(APPEND priv_req single_screen_avi_play)
endif()

if (CONFIG_SINGLE_SCREEN_FONT_DISPLAY)
list(APPEND priv_req single_screen_font_display)
endif()

armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES "${priv_req}"
)