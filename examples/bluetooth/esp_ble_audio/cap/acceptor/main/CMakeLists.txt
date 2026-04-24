set(srcs "main.c")

if(CONFIG_EXAMPLE_UNICAST)
    list(APPEND srcs "cap_acceptor_unicast.c")
endif()

if(CONFIG_EXAMPLE_BROADCAST)
    list(APPEND srcs "cap_acceptor_broadcast.c")
endif()

idf_component_register(SRCS "${srcs}"
                       REQUIRES bt nvs_flash)
