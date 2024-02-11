set(COMPONENT_REQUIRES  nvs_flash mbedtls freertos esp_system log)
idf_component_register(SRCS "storage.c"
                        INCLUDE_DIRS "include"
                        REQUIRES ${COMPONENT_REQUIRES})

# 