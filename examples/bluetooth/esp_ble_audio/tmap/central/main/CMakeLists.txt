set(srcs "cap_initiator.c"
         "ccp_server.c"
         "mcp_server.c"
         "vcp_vol_ctlr.c"
         "main.c")

idf_component_register(SRCS "${srcs}"
                       REQUIRES bt nvs_flash)
