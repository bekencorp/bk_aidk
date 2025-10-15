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

if (CONFIG_WANSON_CN_LICENSE AND CONFIG_SYS_CPU0)
	list(APPEND srcs
		src/bk_wanson_usrkey_http.c
	)
endif()

armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common include avdk_utils audio_record wanson lwip_intf_v2_1 mbedtls psa_mbedtls
)

