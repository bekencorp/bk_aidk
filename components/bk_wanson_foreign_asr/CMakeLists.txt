set(incs include model)
set(srcs "")

if (CONFIG_WANSON_FOREIGN_ASR)
	list(APPEND srcs
		src/bk_wanson_asr.c
	)

	set(GENERATED_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/src/foreign_model.S 
	${CMAKE_CURRENT_LIST_DIR}/src/foreign_model.c
	${CMAKE_CURRENT_LIST_DIR}/src/foreign_model.h
    )
     
    add_custom_command(
        OUTPUT ${GENERATED_SOURCES}
        COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/src/foreign_model.py  
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/src/
        COMMENT "Running foreign_model.py to generate foreign_model.S and foreign_model.c"
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/foreign_model.py   
    )
    list(APPEND srcs ${GENERATED_SOURCES})
endif()

if (CONFIG_BEKEN_WANSON_ASR_TEST)
	list(APPEND srcs
		src/bk_wanson_asr_test.c
	)
endif()

armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common include avdk_utils audio_record wanson_foreign
)

