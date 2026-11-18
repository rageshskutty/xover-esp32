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

add_library(dspgraph_json STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspgraph/dspgraph_json.c
)
target_include_directories(dspgraph_json PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspgraph/internal
)
target_link_libraries(dspgraph_json PUBLIC cjson)

add_library(dspblocks STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/dspblocks.c
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/dsp_sos_rt.c
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/blocks_rt.c
)
target_include_directories(dspblocks PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/include
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
add_executable(test_graph_json tests/test_graph_json.c)
target_link_libraries(test_graph_json PRIVATE dspgraph_json)
add_test(NAME test_graph_json COMMAND test_graph_json)

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
