set(COMPONENT_REQUIRES )
idf_component_register(SRCS "header.c"
                        INCLUDE_DIRS "include"
                        REQUIRES ${COMPONENT_REQUIRES})