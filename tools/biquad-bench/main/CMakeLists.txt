set(BENCH_SRCS "bench_main.c" "biquad_ref.c" "bench_util.c")
set(BENCH_REQ spi_flash esp_psram esp_hw_support
    esp_wifi esp_netif esp_event nvs_flash lwip esp_timer)

# Wi-Fi load build (sdkconfig.wifi): the benchmark code is compiled unchanged;
# wifi_load.c supplies app_main, brings up a SoftAP and a TCP load server on
# core 0, then calls the benchmark's own entry point.
if(CONFIG_BENCH_WIFI_LOAD)
    list(APPEND BENCH_SRCS "wifi_load.c")
endif()

idf_component_register(
    SRCS ${BENCH_SRCS}
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${BENCH_REQ}
)

if(CONFIG_BENCH_WIFI_LOAD)
    set_source_files_properties("bench_main.c" PROPERTIES
        COMPILE_DEFINITIONS "app_main=bench_app_main")
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)

option(BENCH_WERROR "Treat warnings as errors" OFF)
if(BENCH_WERROR)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Werror)
endif()
