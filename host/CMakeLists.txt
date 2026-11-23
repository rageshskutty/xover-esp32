cmake_minimum_required(VERSION 3.16)
project(xover_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wconversion -Werror)
endif()

option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
if(ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

set(CJSON_DIR ${CMAKE_CURRENT_LIST_DIR}/../firmware/third_party/cjson)
add_library(cjson STATIC ${CJSON_DIR}/cJSON.c)
target_include_directories(cjson SYSTEM PUBLIC ${CJSON_DIR})
if(MSVC)
    target_compile_options(cjson PRIVATE /W3)
else()
    target_compile_options(cjson PRIVATE -w)
endif()

add_library(dspblocks STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/dspblocks.c
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/dsp_sos_rt.c
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/blocks_rt.c
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/dsp_s7_ramp.c
)
target_include_directories(dspblocks PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/include
)
target_include_directories(dspblocks PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/internal
)

add_library(dspblocks_eval SHARED
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/dspblocks.c
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/dsp_sos_rt.c
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/blocks_rt.c
)
set_target_properties(dspblocks_eval PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(dspblocks_eval PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/include
)
if(NOT MSVC)
    target_link_libraries(dspblocks_eval PRIVATE m)
endif()

enable_testing()

add_executable(test_sos
    tests/test_sos.c
)
target_link_libraries(test_sos PRIVATE dspblocks)

if(NOT MSVC)
    target_link_libraries(test_sos PRIVATE m)
endif()

add_test(NAME test_sos COMMAND test_sos)

add_executable(test_design
    tests/test_design.c
)
target_include_directories(test_design PRIVATE
    tests
)
target_link_libraries(test_design PRIVATE dspblocks)

if(NOT MSVC)
    target_link_libraries(test_design PRIVATE m)
endif()

add_test(NAME test_design COMMAND test_design)

add_executable(test_eval
    tests/test_eval.c
)
target_include_directories(test_eval PRIVATE
    tests
)
target_link_libraries(test_eval PRIVATE dspblocks)

if(NOT MSVC)
    target_link_libraries(test_eval PRIVATE m)
endif()

add_test(NAME test_eval COMMAND test_eval)

add_executable(test_blocks tests/test_blocks.c)
target_include_directories(test_blocks PRIVATE tests)
target_link_libraries(test_blocks PRIVATE dspblocks)
if(NOT MSVC)
    target_link_libraries(test_blocks PRIVATE m)
endif()
add_test(NAME test_blocks COMMAND test_blocks)
add_executable(test_eval_analytic tests/test_eval_analytic.c)
target_include_directories(test_eval_analytic PRIVATE tests)
target_link_libraries(test_eval_analytic PRIVATE dspblocks)
if(NOT MSVC)
    target_link_libraries(test_eval_analytic PRIVATE m)
endif()
add_test(NAME test_eval_analytic COMMAND test_eval_analytic)

add_executable(test_f32 tests/test_f32.c)
target_link_libraries(test_f32 PRIVATE dspblocks)
if(NOT MSVC)
    target_link_libraries(test_f32 PRIVATE m)
endif()
add_test(NAME test_f32 COMMAND test_f32)

add_library(tdm_pack STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/tdmout/tdm_pack.c
)
target_include_directories(tdm_pack PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/tdmout/internal
)
add_executable(test_tdm_pack tests/test_tdm_pack.c)
target_link_libraries(test_tdm_pack PRIVATE tdm_pack)
add_test(NAME test_tdm_pack COMMAND test_tdm_pack)

add_executable(test_sos_response tests/test_sos_response.c)
target_link_libraries(test_sos_response PRIVATE dspblocks)
if(NOT MSVC)
    target_link_libraries(test_sos_response PRIVATE m)
endif()
add_test(NAME test_sos_response COMMAND test_sos_response)

add_executable(test_s7_ramp tests/test_s7_ramp.c)
target_include_directories(test_s7_ramp PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/internal
)
target_link_libraries(test_s7_ramp PRIVATE dspblocks)
if(NOT MSVC)
    target_link_libraries(test_s7_ramp PRIVATE m)
endif()
add_test(NAME test_s7_ramp COMMAND test_s7_ramp)

# pthread.h and C11 atomics are not available with MSVC; the ring test is built on Linux/CI only.
if(NOT MSVC)
    find_package(Threads REQUIRED)
    add_library(audiosrc_ring STATIC
        ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/audiosrc/audiosrc_ring.c
    )
    target_include_directories(audiosrc_ring PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/audiosrc/internal
    )
    add_executable(test_spsc_ring tests/test_spsc_ring.c)
    target_link_libraries(test_spsc_ring PRIVATE audiosrc_ring Threads::Threads)
    add_test(NAME test_spsc_ring COMMAND test_spsc_ring)
endif()
