idf_component_register(
    SRCS "tdmout.c" "tdm_pack.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "internal" "../../main"
    REQUIRES driver hal
)
