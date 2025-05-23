function(set_local_library_compile_option input_target)
    set(local_compile_option )

    set(TMP_WARNING_LIST)
    list(APPEND TMP_WARNING_LIST
                unused-but-set-variable
                unused-variable
                unused-function
                unused-but-set-variable
                unused-label
                #uninitialized
    )

    foreach(tmp_loopvar IN LISTS TMP_WARNING_LIST)
        list(APPEND local_compile_option -Wno-error=${tmp_loopvar} -Wno-${tmp_loopvar})
    endforeach()

    target_compile_options(${input_target} PRIVATE ${local_compile_option})
endfunction()

set(incs . include)
set(srcs)

list(APPEND srcs
	app_main.c
)

if (CONFIG_SYS_CPU0)
	list(APPEND srcs
	    app_audio_arbiter.c
        audio_para.c
        app_evt_process.c
    )
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()

armino_component_register(SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES bk_init lwip_intf_v2_1 agora-iot-sdk media_service multimedia avdk_utils mbedtls json lvgl bk_vfs wanson bk_wanson_asr bk_factory_config display_service audio_play audio_record ota bk_countdown bk_led_blink bk_motor
    bk_wanson_asr bk_factory_config bk_nfc bk_app_event bk_countdown bk_led_blink bk_motor bk_boarding_service
    bk_smart_config bk_bt bk_key_app asr audio_engine video_engine network_transfer
)

message("zczczczc ${srcs}")

#get_filename_component(config_component_name ${CMAKE_CURRENT_LIST_DIR} NAME)
#armino_component_get_target(target "${config_component_name}")
#set_local_library_compile_option(${target})
