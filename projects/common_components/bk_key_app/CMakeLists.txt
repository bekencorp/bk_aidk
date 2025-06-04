set(incs .)
set(srcs "")

list(APPEND srcs
	key_app_service.c
)


armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_init lwip_intf_v2_1 agora-iot-sdk media_service multimedia avdk_utils 
				  mbedtls psa_mbedtls json lvgl bk_vfs wanson bk_wanson_asr bk_factory_config display_service 
				  ota bk_nfc bk_app_event bk_countdown bk_led_blink bk_motor key main 
				  bk_boarding_service bk_smart_config bk_bt video_engine audio_engine network_transfer VolcEngineRTCLite
)
