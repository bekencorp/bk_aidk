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

set(incs .)
set(srcs)

if (CONFIG_SYS_CPU0)

if (CONFIG_BT)

if (CONFIG_NET_PAN)

list(APPEND incs
    pan
)

list(APPEND srcs
    pan/bt_manager.c
    pan/pan_service.c
    pan/pan_demo_cli.c
    pan/bt_comm_list.c
    pan/hidd_service.c
)

endif() #CONFIG_NET_PAN

if (CONFIG_A2DP_SINK_DEMO)

    list(APPEND incs
        a2dp_sink
    )

    list(APPEND srcs
        a2dp_sink/a2dp_sink_demo.c
        a2dp_sink/ring_buffer_node.c
        a2dp_sink/mpeg4_latm_dec.c
        a2dp_sink_demo_cli.c
		bt_manager.c
    )
endif() #CONFIG_A2DP_SINK_DEMO

if (CONFIG_HFP_HF_DEMO)
    list(APPEND incs
        hfp_hf
    )

    list(APPEND srcs
        hfp_hf/hfp_hf_demo.c
        hfp_hf/ring_buffer_particle.c
    )
endif() #CONFIG_HFP_HF_DEMO

endif() #CONFIG_BT

if (CONFIG_BLE)

endif() #CONFIG_BLE

if (CONFIG_BT OR CONFIG_BLE)

list(APPEND incs
    storage
)

list(APPEND srcs
    storage/bluetooth_storage.c
)

endif() #CONFIG_BT OR CONFIG_BLE

endif() #CONFIG_SYS_CPU0

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES multimedia avdk_utils bk_factory_config lwip_intf_v2_1 bk_app_event audio_play audio_record)

#get_filename_component(config_component_name ${CMAKE_CURRENT_LIST_DIR} NAME)
#armino_component_get_target(target "${config_component_name}")
#set_local_library_compile_option(${target})

