set(COMPONENT_REQUIRES freertos esp_system esp_wifi log nvs_flash lwip)
idf_component_register(SRCS "wifi.c"
                        INCLUDE_DIRS "include"
                        REQUIRES ${COMPONENT_REQUIRES})