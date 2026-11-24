cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xover_esp32)

# CI passes -DXOVER_WERROR=ON (AGENTS C3). Scoped to this repository's own
# components so ESP-IDF and managed components keep their upstream warning policy.
option(XOVER_WERROR "Treat warnings in xover-esp32 components as errors" OFF)
if(XOVER_WERROR)
    foreach(xover_component IN ITEMS main audiosrc dspblocks pcm5122 tdmout xoweb)
        idf_component_get_property(xover_lib ${xover_component} COMPONENT_LIB)
        target_compile_options(${xover_lib} PRIVATE -Werror)
    endforeach()
endif()
