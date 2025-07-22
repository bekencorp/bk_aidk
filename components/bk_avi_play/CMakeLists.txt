set(incs .)

if (CONFIG_AVI_PLAY)
if (CONFIG_PSRAM)
set(srcs
	bk_avi_play.c
	)
else()
set(srcs
	bk_avi_play_no_psram.c
	)
endif()
endif()

armino_component_register(SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES include lvgl multimedia bk_vfs
	)
