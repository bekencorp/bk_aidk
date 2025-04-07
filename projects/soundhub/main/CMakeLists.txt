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

set(incs . include asr)
set(srcs)

list(APPEND srcs
	app_main.c
)

if (CONFIG_SYS_CPU0)
	list(APPEND srcs
	    app_event.c
	    app_audio_arbiter.c
	    agora_rtc_demo.c
	    agora_sdk/agora_rtc.c
	    audio/audio_transfer.c
	    boarding_service/boarding_core.c
	    boarding_service/boarding_service.c
	    boarding_service/wifi_boarding_utils.c
	    agora_debug.c
	    led_blink.c
	    countdown.c
	    motor.c
	)

if (CONFIG_NETWORK_AUTO_RECONNECT)
    list(APPEND srcs
       smart_config/bk_genie_smart_config.c
    )
endif()


if (CONFIG_BT)

    list(APPEND incs
        bt
    )

	list(APPEND srcs
	    bt/bt_manager.c
	    bt/storage/bluetooth_storage.c
	    bt/bt_comm_list.c
	)


if (CONFIG_NET_PAN)
	list(APPEND srcs
	    bt/pan_service.c
	    bt/pan_demo_cli.c
	)
endif()

if (CONFIG_A2DP_SINK_DEMO)

    list(APPEND incs
        bt/a2dp_sink
    )

    list(APPEND srcs
        bt/a2dp_sink/a2dp_sink_demo.c
        bt/a2dp_sink/ring_buffer_node.c
        bt/a2dp_sink/mpeg4_latm_dec.c
        bt/a2dp_sink_demo_cli.c
        bt/hfp_hf/hfp_hf_demo.c
        bt/hfp_hf/ring_buffer_particle.c
    )
endif()

if (CONFIG_HFP_HF_DEMO)
    list(APPEND incs
        bt/hfp_hf
    )

    list(APPEND srcs
        bt/hfp_hf/hfp_hf_demo.c
        bt/hfp_hf/ring_buffer_particle.c
    )
endif()

endif()

if (CONFIG_BLE OR CONFIG_BT)
    list(APPEND incs
        bt/storage
    )

    list(APPEND srcs
        bt/storage/bluetooth_storage.c
    )
endif()

endif()


if (CONFIG_SYS_CPU1)
    list(APPEND srcs
        agora_debug.c
    )
endif()

list(APPEND srcs
    display/avi_play.c
    display/ota_display.c
)

if (CONFIG_SYS_CPU2)
    list(APPEND srcs
        asr/armino_asr.c
    )
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()

armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES bk_init lwip_intf_v2_1 agora-iot-sdk media_service multimedia avdk_utils mbedtls json lvgl bk_vfs wanson bk_wanson_asr bk_factory_config display_service audio_play audio_record ota
)

get_filename_component(config_component_name ${CMAKE_CURRENT_LIST_DIR} NAME)
armino_component_get_target(target "${config_component_name}")
set_local_library_compile_option(${target})
