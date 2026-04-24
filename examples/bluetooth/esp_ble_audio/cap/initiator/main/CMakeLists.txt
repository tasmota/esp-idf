set(srcs "cap_initiator_tx.c"
         "main.c")

if(CONFIG_EXAMPLE_UNICAST)
    list(APPEND srcs "cap_initiator_unicast.c")
endif()

if(CONFIG_EXAMPLE_BROADCAST)
    list(APPEND srcs "cap_initiator_broadcast.c")
endif()

idf_component_register(SRCS "${srcs}"
                       REQUIRES bt nvs_flash)
