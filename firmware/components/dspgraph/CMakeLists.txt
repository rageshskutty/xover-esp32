idf_component_register(
    SRCS "dspgraph.c" "dspgraph_json.c" "dspgraph_validate.c" "${CMAKE_CURRENT_LIST_DIR}/../../third_party/cjson/cJSON.c"
    INCLUDE_DIRS "include" "internal" "${CMAKE_CURRENT_LIST_DIR}/../../third_party/cjson"
)
