set(srcs "i3c_i2c_basic_main.c")

idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES esp_driver_i3c
                       INCLUDE_DIRS ".")
