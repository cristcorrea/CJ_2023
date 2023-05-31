set(COMPONENT_REQUIRES mqtt  lwip  esp_netif esp_event esp_system freertos log)
idf_component_register(SRCS "mqtt.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES BLUFI HEADER STORAGE RTC POMP SOIL
                        REQUIRES ${COMPONENT_REQUIRES}
                        EMBED_TXTFILES hivemq_certificate.pem)

#
