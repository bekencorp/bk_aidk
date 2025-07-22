if (CONFIG_DUAL_SCREEN_AVI_PLAY)

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
    lv_font.c
)
endif()

if (NOT CONFIG_PSRAM)
list(APPEND srcs
    avi_play_no_psram.c
)
endif()
endif()

armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES multimedia lvgl bk_avi_play bk_vfs ota
)