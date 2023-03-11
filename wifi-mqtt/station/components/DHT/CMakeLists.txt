set(COMPONENT_REQUIRES driver)
idf_component_register(SRCS "dht.c"
                        INCLUDE_DIRS "include"
                        REQUIRES ${COMPONENT_REQUIRES})