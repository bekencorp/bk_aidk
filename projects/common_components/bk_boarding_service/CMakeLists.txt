set(incs .)
set(srcs)

if (CONFIG_SYS_CPU0)
if (CONFIG_BK_BOARDING_SERVICE AND CONFIG_BLE)
list(APPEND srcs
    boarding_core.c
    boarding_service.c
    wifi_boarding_utils.c
)
endif()
endif()

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES bk_smart_config bk_bt json bk_app_event bk_factory_config agora-iot-sdk main network_transfer VolcEngineRTCLite)
