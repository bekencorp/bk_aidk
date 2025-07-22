if (CONFIG_SINGLE_SCREEN_FONT_DISPLAY)

set(incs .)
set(srcs "")

if (CONFIG_PSRAM)
list(APPEND srcs
    ota_display.c
)
endif()

if (CONFIG_SYS_CPU0)
list(APPEND srcs
    lvgl_app.c
)
endif()

if (CONFIG_LVGL)
list(APPEND srcs
    lvgl_ui.c
    lv_comm_list.c
)
endif()
endif()

armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES multimedia lvgl bk_vfs ota display_service
)