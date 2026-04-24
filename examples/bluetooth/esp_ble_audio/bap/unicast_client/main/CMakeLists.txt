set(srcs "stream_tx.c"
         "main.c")

idf_component_register(SRCS "${srcs}"
                       REQUIRES bt nvs_flash)
