set(COMPONENT_REQUIRES  soc  esp_adc freertos log)
idf_component_register(SRCS "soil.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES HEADER
                        REQUIRES ${COMPONENT_REQUIRES})

# 