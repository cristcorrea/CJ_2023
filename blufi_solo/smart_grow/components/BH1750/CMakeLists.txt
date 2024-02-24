set(COMPONENT_REQUIRES  driver spi_flash)
idf_component_register(SRCS "bh1750.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES HEADER
                        REQUIRES ${COMPONENT_REQUIRES})