set(incs include)
set(srcs "")

if (CONFIG_BEKEN_WANSON_ASR)
	list(APPEND srcs
		src/bk_wanson_asr.c
	)
endif()

if (CONFIG_BEKEN_WANSON_ASR_TEST)
	list(APPEND srcs
		src/bk_wanson_asr_test.c
	)
endif()

armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common include avdk_utils audio_record wanson
)

