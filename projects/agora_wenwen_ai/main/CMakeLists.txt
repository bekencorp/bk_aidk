set(incs . include asr ../../agora_wanson_ai/main ../../agora_wanson_ai/main/include)
set(srcs)

list(APPEND srcs
	app_main.c
)

if (CONFIG_SYS_CPU0)
list(APPEND srcs
    ../../agora_wanson_ai/main/agora_rtc_demo.c
    ../../agora_wanson_ai/main/agora_sdk/agora_rtc.c
    ../../agora_wanson_ai/main/audio/audio_transfer.c
)
endif()

if (CONFIG_SYS_CPU1)
list(APPEND srcs
	../../agora_wanson_ai/main/audio/play_voice.c
	../../agora_wanson_ai/main/audio/audio_control.c
	asr/wenwen_asr.c
)
endif()#(CONFIG_SYS_CPU1)

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()


armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES bk_init lwip_intf_v2_1 agora-iot-sdk avdk_libs media_service multimedia avdk_utils audio_play audio_record wenwen)
