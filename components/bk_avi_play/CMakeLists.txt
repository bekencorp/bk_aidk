set(incs .)

if (CONFIG_AVI_PLAY)
set(srcs
	bk_avi_play.c
	)
endif()

armino_component_register(SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES include lvgl
	)
