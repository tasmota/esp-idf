set(srcs "main.c")

idf_component_register(SRCS "${srcs}"
                       REQUIRES bt nvs_flash)
