# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENTS_DIRS 
    $ENV{ARMINO_AVDK_DIR}/components
    $ENV{ARMINO_AVDK_DIR}/components/audio_algorithms
    $ENV{ARMINO_AVDK_DIR}/components/codec/
    $ENV{ARMINO_AVDK_DIR}/components/codec/legacy 
    $ENV{ARMINO_AVDK_DIR}/components/codec/legacy/media/audio
    $ENV{ARMINO_AVDK_DIR}/components/codec/legacy/media
    $ENV{ARMINO_AVDK_DIR}/components/bk_thirdparty/agora-iot-sdk
    $ENV{ARMINO_AVDK_DIR}/components/bk_thirdparty
    $ENV{ARMINO_AVDK_DIR}/components/multimedia
	$ENV{ARMINO_AVDK_DIR}/components/avdk_utils
    $ENV{ARMINO_AVDK_DIR}/properties/modules 
    $ENV{ARMINO_AVDK_DIR}/properties/modules/video_codec
    $ENV{ARMINO_AVDK_DIR}/properties/modules/audio_codec
	$ENV{ARMINO_AVDK_DIR}/components/bk_thirdparty/asr
	$ENV{ARMINO_AVDK_DIR}/components/bk_thirdparty/asr/wanson
	$ENV{ARMINO_AVDK_DIR}/../components/bk_wanson_asr
	$ENV{ARMINO_AVDK_DIR}/../components/bk_factory_config
    $ENV{ARMINO_AVDK_DIR}/../components/bk_genie_core
	$ENV{ARMINO_AVDK_DIR}/../properties/bk_aidk_private
    )

include($ENV{ARMINO_PATH}/tools/build_tools/cmake/project.cmake)
project(volc_rtc)
