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

add_library(dspblocks STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/dspblocks.c
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/dsp_sos_rt.c
)
target_include_directories(dspblocks PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/components/dspblocks/include
)

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
