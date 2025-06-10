# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENTS_DIRS 
    $ENV{ARMINO_PATH}/../components
    $ENV{ARMINO_PATH}/../components/audio_algorithms
    $ENV{ARMINO_PATH}/../components/codec/
	$ENV{ARMINO_PATH}/../components/include/avdk
    $ENV{ARMINO_PATH}/../components/bk_thirdparty/agora-iot-sdk
    $ENV{ARMINO_PATH}/../components/bk_thirdparty
    $ENV{ARMINO_PATH}/../components/multimedia
    $ENV{ARMINO_PATH}/../components/avdk_utils
    $ENV{ARMINO_PATH}/../properties/modules 
    $ENV{ARMINO_PATH}/../properties/modules/video_codec
    $ENV{ARMINO_PATH}/../properties/modules/audio_codec
    $ENV{ARMINO_PATH}/../components/bk_thirdparty/asr
    $ENV{ARMINO_PATH}/../components/bk_thirdparty/asr/wanson
    $ENV{ARMINO_PATH}/../../components/bk_wanson_asr
    $ENV{ARMINO_PATH}/../../components/bk_avi_play
    $ENV{ARMINO_PATH}/../../components/bk_factory_config
    $ENV{ARMINO_PATH}/../../properties/bk_aidk_private
    $ENV{ARMINO_PATH}/components/base64
    $ENV{ARMINO_PATH}/components/bk_modem
    $ENV{ARMINO_PATH}/../../projects/common_components/bk_led_blink
    $ENV{ARMINO_PATH}/../../projects/common_components/bk_motor
    $ENV{ARMINO_PATH}/../../projects/common_components/bk_countdown
    $ENV{ARMINO_PATH}/../../projects/common_components/single_screen_avi_play
    $ENV{ARMINO_PATH}/../../projects/common_components/bk_app_event
    $ENV{ARMINO_PATH}/../../projects/common_components/dual_screen_avi_play
    $ENV{ARMINO_PATH}/../../projects/common_components/bk_boarding_service
    $ENV{ARMINO_PATH}/../../projects/common_components/bk_smart_config
    $ENV{ARMINO_PATH}/../../projects/common_components/bk_bt
    $ENV{ARMINO_PATH}/../../projects/common_components/audio_engine
    $ENV{ARMINO_PATH}/../../projects/common_components/video_engine
    $ENV{ARMINO_PATH}/../../projects/common_components/network_transfer
    $ENV{ARMINO_PATH}/../../projects/common_components/asr
    $ENV{ARMINO_PATH}/../../projects/common_components/bk_key_app
    )

include($ENV{ARMINO_PATH}/tools/build_tools/cmake/project.cmake)
project(ai_dashboard)
