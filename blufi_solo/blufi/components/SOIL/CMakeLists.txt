set(COMPONENT_REQUIRES freertos soc log esp_adc )
idf_component_register(SRCS "soil.c"
                        INCLUDE_DIRS "include"
                        REQUIRES ${COMPONENT_REQUIRES})