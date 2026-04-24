set(srcs "bap_broadcast_sink.c"
         "vcp_vol_renderer.c"
         "main.c")

idf_component_register(SRCS "${srcs}"
                       REQUIRES bt nvs_flash)
