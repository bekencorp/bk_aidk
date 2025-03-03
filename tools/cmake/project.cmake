# Designed to be included from an ARMINO app's CMakeLists.txt file
cmake_minimum_required(VERSION 3.5)

include($ENV{ARMINO_PATH}/../tools/cmake/project.cmake)

set(EXTRA_COMPONENTS_DIRS
    $ENV{ARMINO_PATH}/../components)

list(APPEND EXTRA_COMPONENTS_DIRS ${AVDK_COMPONENTS_DIRS})
#include($ENV{ARMINO_PATH}/tools/build_tools/cmake/project.cmake)
