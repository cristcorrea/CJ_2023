set(COMPONENT_REQUIRES mqtt log lwip freertos esp_netif esp_event esp_system)
idf_component_register(SRCS "mqtt.c"
                        INCLUDE_DIRS "include"
                        REQUIRES ${COMPONENT_REQUIRES})