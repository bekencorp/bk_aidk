# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENTS_DIRS 
    $ENV{ARMINO_PATH}/../components
    $ENV{ARMINO_PATH}/../components/bk_audio
    $ENV{ARMINO_PATH}/../components/bk_audio/audio_streams
    $ENV{ARMINO_PATH}/../components/codec/
	$ENV{ARMINO_PATH}/../components/include/avdk
    $ENV{ARMINO_PATH}/../components/bk_thirdparty/asr
    $ENV{ARMINO_PATH}/../components/bk_thirdparty/zlib
    $ENV{ARMINO_PATH}/../properties/modules
    $ENV{ARMINO_PATH}/../properties/modules/video_codec
    $ENV{ARMINO_PATH}/../properties/modules/audio_codec
    $ENV{ARMINO_PATH}/../../components
    )

include($ENV{ARMINO_PATH}/tools/build_tools/cmake/project.cmake)
project(app)


armino_build_set_property(COMPILE_OPTIONS "-Wno-unused-variable" APPEND)
armino_build_set_property(COMPILE_OPTIONS "-Wno-unused-but-set-variable" APPEND)
armino_build_set_property(COMPILE_OPTIONS "-Wno-sign-compare" APPEND)
armino_build_set_property(COMPILE_OPTIONS "-Wno-permissive" APPEND)
armino_build_set_property(CXX_COMPILE_OPTIONS "-fpermissive" APPEND)
armino_build_set_property(CXX_COMPILE_OPTIONS "-Wno-permissive" APPEND)
armino_build_set_property(COMPILE_OPTIONS "-Wno-switch" APPEND)

#armino_build_set_property(COMPILE_DEFINITIONS "-DUART_BAUD_RATE=2000000" APPEND)