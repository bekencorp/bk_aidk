set(incs . include)
set(srcs)

list(APPEND srcs
	app_main.c
)

if (0)
list(APPEND srcs
    websocket_rtc_demo.c
    beken_sdk/beken_rtc.c
    beken_debug.c
)
if (CONFIG_NETWORK_AUTO_RECONNECT)
list(APPEND srcs
   smart_config/bk_genie_smart_config.c
)
endif()
endif()

if (CONFIG_VOLC_RTC_DEMO_ENABLE)

list(APPEND incs
            VolcEngineRTCLiteDemo/src/file_parser/src
            VolcEngineRTCLiteDemo/src/file_parser/include
            VolcEngineRTCLiteDemo/src/utility
)

list(APPEND srcs
    VolcEngineRTCLiteDemo/src/VolcEngineRTCLiteDemo.c
    VolcEngineRTCLiteDemo/src/file_parser/src/audio_parser_aac.c
    VolcEngineRTCLiteDemo/src/file_parser/src/audio_parser_g711.c
    VolcEngineRTCLiteDemo/src/file_parser/src/audio_parser_pcm.c
    VolcEngineRTCLiteDemo/src/file_parser/src/file_parser.c
    VolcEngineRTCLiteDemo/src/file_parser/src/utils.c
    VolcEngineRTCLiteDemo/src/file_parser/src/video_parser_h264.c
    VolcEngineRTCLiteDemo/src/file_parser/src/video_parser_jpeg.c
    VolcEngineRTCLiteDemo/src/file_parser/src/video_parser_yuv420.c
    VolcEngineRTCLiteDemo/src/utility/file_writer.c
    VolcEngineRTCLiteDemo/src/utility/pacer.c
    VolcEngineRTCLiteDemo/src/utility/utility.c
)
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()


armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES bk_init lwip_intf_v2_1 VolcEngineRTCLite media_service multimedia avdk_utils mbedtls json lvgl bk_vfs wanson bk_wanson_asr bk_factory_config display_service ota bk_nfc)

