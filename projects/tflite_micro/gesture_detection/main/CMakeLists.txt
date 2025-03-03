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
        app_main.c
        # ${TF_SOURCES}
    )

    list(APPEND incs
        include
        # tflite
    )

elseif (CONFIG_SYS_CPU1)
	message("build cpu1")

    file(GLOB_RECURSE TF_SOURCES tflite/*.c tflite/*.cc)

    list(APPEND srcs
            app_cpu1_main.c
            app_main_cpu1.cc
            ${TF_SOURCES}
        )
    
    list(APPEND incs
            tflite
        )
        
elseif (CONFIG_SYS_CPU2)
	message("build cpu1")
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()

armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"	
)


