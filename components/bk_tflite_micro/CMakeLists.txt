set(incs )
set(srcs )

# cmake_minimum_required(VERSION 3.5)
if (CONFIG_TFLITE_MICRO)

set(TENSORFLOW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/tflite-micro)

set(MAKEFILE_DIR ${TENSORFLOW_ROOT}/tensorflow/lite/micro/tools/make)

set(DOWNLOADS_DIR ${MAKEFILE_DIR}/downloads)

set(BASE_GENDIR ${TENSORFLOW_ROOT}/gen)

list(APPEND incs 
	${TENSORFLOW_ROOT}
	${DOWNLOADS_DIR}/gemmlowp
	${DOWNLOADS_DIR}/flatbuffers/include
	${DOWNLOADS_DIR}/kissfft
	${DOWNLOADS_DIR}/ruy
)
endif()

armino_component_register(INCLUDE_DIRS "${incs}")

if (CONFIG_TFLITE_MICRO)

target_compile_options(${COMPONENT_LIB} INTERFACE -DTF_LITE_STATIC_MEMORY)

include(ExternalProject)

set(TFLM_LIB_DIR ${BASE_GENDIR}/cortex_m_generic_cortex-m33_default_cmsis_nn_gcc/lib/libtensorflow-microlite.a)

set(target "cortex_m_generic")
set(target_arch "cortex-m33")
set(optimized_kernel_dir "cmsis_nn")

add_custom_command(OUTPUT ${TFLM_LIB_DIR}
    COMMAND rm -rf ${BASE_GENDIR}
	COMMAND make -f ${MAKEFILE_DIR}/Makefile TARGET=${target} TARGET_ARCH=${target_arch} OPTIMIZED_KERNEL_DIR=${optimized_kernel_dir} microlite -j16
	WORKING_DIRECTORY ${TENSORFLOW_ROOT}
    VERBATIM
    COMMENT "Build tflm project"
)
add_custom_target(tflm_project DEPENDS ${TFLM_LIB_DIR})

add_library(tflm STATIC IMPORTED GLOBAL)
add_dependencies(tflm tflm_project)
set_target_properties(tflm PROPERTIES IMPORTED_LOCATION ${TFLM_LIB_DIR})
target_link_libraries(${COMPONENT_LIB} INTERFACE tflm)
endif()
