set(incs .)
set(srcs "")

if (CONFIG_SYS_CPU2)
list(APPEND srcs
    armino_asr.c
)
endif()

set(priv_req bk_common multimedia bk_boarding_service bk_smart_config avdk_utils bk_wanson_asr bk_wanson_foreign_asr bk_unisound_asr)

armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES "${priv_req}"
)