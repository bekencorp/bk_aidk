set(incs .)
set(srcs "")

list(APPEND srcs
	video_engine.c
	video_dump_data.c
)

armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common multimedia bk_boarding_service bk_smart_config avdk_utils
)