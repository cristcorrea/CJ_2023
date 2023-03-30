set(COMPONENT_REQUIRES esp_system esp_event log esp_common nvs_flash 
    esp_netif lwip)
idf_component_register(SRCS "rtc.c"
                        INCLUDE_DIRS "include"
                        REQUIRES ${COMPONENT_REQUIRES})