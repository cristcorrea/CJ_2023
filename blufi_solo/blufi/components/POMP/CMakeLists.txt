set(COMPONENT_REQUIRES driver freertos)
idf_component_register(SRCS "pomp.c"
                        INCLUDE_DIRS "include"
                        REQUIRES ${COMPONEN_REQUIRES})