set(incs . include)
set(srcs)

list(APPEND srcs
	app_main.c
)

if (0)
list(APPEND srcs
    websocket_rtc_demo.c
    beken_sdk/beken_rtc.c
    beken_debug.c
)
if (CONFIG_NETWORK_AUTO_RECONNECT)
list(APPEND srcs
   smart_config/bk_genie_smart_config.c
)
endif()
endif()

if (CONFIG_SYS_CPU1)
list(APPEND srcs
#    beken_debug.c
)
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()


armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES bk_init avdk_utils )
