set(COMPONENT_REQUIRES  soc  esp_adc freertos log hal driver)
idf_component_register(SRCS "soil.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES HEADER
                        REQUIRES ${COMPONENT_REQUIRES})

# 