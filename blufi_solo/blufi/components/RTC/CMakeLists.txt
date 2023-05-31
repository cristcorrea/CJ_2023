set(COMPONENT_REQUIRES newlib  esp_event nvs_flash esp_system  log esp_common
    esp_netif lwip)
idf_component_register(SRCS "ntp.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES HEADER
                        REQUIRES ${COMPONENT_REQUIRES})

#