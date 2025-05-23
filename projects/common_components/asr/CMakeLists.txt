set(incs .)
set(srcs "")

if (CONFIG_SYS_CPU2)
list(APPEND srcs
    armino_asr.c
)
endif()

armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common multimedia bk_boarding_service bk_smart_config avdk_utils bk_wanson_asr
)