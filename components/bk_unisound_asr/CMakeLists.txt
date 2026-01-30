set(incs include)
set(srcs "")

if (CONFIG_BEKEN_UNISOUND_ASR)
	list(APPEND srcs
		src/bk_unisound_asr.c
		src/bk_unisound_authcode.c
	)
endif()

if (CONFIG_BEKEN_UNISOUND_ASR_TEST)
	list(APPEND srcs
		src/bk_unisound_asr_test.c
	)
endif()

if (CONFIG_UNISOUND_LICENSE)
	list(APPEND srcs
		src/bk_unisound_usrkey_http.c
		src/bk_unisound_authcode.c
	)
endif()

armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common include avdk_utils audio_record unisound lwip_intf_v2_1 psa_mbedtls json app
)
