idf_component_register(SRCS "dshot_esc_example_main.c" "dshot_esc_encoder.c"
                       PRIV_REQUIRES esp_driver_rmt
                       INCLUDE_DIRS ".")
