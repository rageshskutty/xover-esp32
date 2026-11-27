idf_component_register(
    SRCS "bench_main.c" "biquad_ref.c" "bench_util.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES spi_flash esp_psram esp_hw_support
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)

option(BENCH_WERROR "Treat warnings as errors" OFF)
if(BENCH_WERROR)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Werror)
endif()