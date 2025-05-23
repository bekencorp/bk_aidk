set(incs
    .
)
set(srcs
    main.c
)

if (CONFIG_SYS_CPU0)
	message("build cpu0")

    file(GLOB_RECURSE TF_SOURCES tflite/*.c tflite/*.cc)

    list(APPEND srcs
        app_cpu0_main.c
        media/media_main.c
        # ${TF_SOURCES}
    )

    list(APPEND incs
        include
        media
        # tflite
    )

elseif (CONFIG_SYS_CPU1)
	message("build cpu1")

    file(GLOB_RECURSE TF_SOURCES tflite/*.c tflite/*.cc)

    list(APPEND srcs
            app_cpu1_main.c
            app_main_cpu1.cc
            media/media_ts.c
            media/media_audio.c
			media/resource/cant_det.c
			media/resource/detecting.c
			media/resource/draw.c
			media/resource/get_ready.c
			media/resource/my_show.c
			media/resource/paper.c
			media/resource/pls_stop_hand.c
			media/resource/rock.c
			media/resource/scissors.c
			media/resource/you_loss.c
			media/resource/you_win.c
			media/resource/your_show.c
            ${TF_SOURCES}
        )

    list(APPEND incs
            tflite
            media
            display
        )

elseif (CONFIG_SYS_CPU2)
	message("build cpu1")
endif()

list(APPEND srcs
	display/lvgl_app.c
)

if (CONFIG_LVGL)
list(APPEND srcs
	display/lv_rock.c
	display/lv_paper.c
	display/lv_scissors.c
	display/lv_custom_font.c
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
	PRIV_REQUIRES multimedia bk_init bk_cli bk_vfs lvgl media_service bk_tflite_micro audio_play
)


