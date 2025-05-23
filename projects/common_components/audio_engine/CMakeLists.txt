set(incs .)
set(srcs "")

list(APPEND srcs
	audio_engine.c
	audio_dump_data.c
)

if (CONFIG_SYS_CPU0)
list(APPEND srcs
	audio_transfer.c
)
endif()

armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common multimedia bk_boarding_service bk_smart_config avdk_utils network_transfer
)